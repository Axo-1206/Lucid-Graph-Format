/// @file sema/src/sema/ImportResolver.cpp
///
/// @brief Implementation of import loading.

#include "ImportResolver.hpp"

#include "core/ast/DeclAST.hpp"
#include "core/diagnostics/DiagCode.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace lucid::diag;

namespace lucid::sema
{

    namespace
    {

        struct Loader
        {
            StringPool &pool;
            ASTArena &arena;
            DiagnosticEngine &diag;
            const CompileOptions &options;

            std::unordered_map<InternedString, ModuleAST *> cache;
            std::unordered_set<InternedString> inProgress;
            std::vector<ModuleAST *> order;

            Loader(StringPool &p, ASTArena &a, DiagnosticEngine &d,
                   const CompileOptions &o)
                : pool(p), arena(a), diag(d), options(o) {}

            /// Load a module by path. Returns nullptr on failure.
            ModuleAST *load(InternedString path)
            {
                // ─── Cache hit ────────────────────────────────────────────
                auto it = cache.find(path);
                if (it != cache.end())
                {
                    return it->second;
                }

                // ─── Cycle ────────────────────────────────────────────────
                if (inProgress.find(path) != inProgress.end())
                {
                    diag.errorAt(DiagCode::Import_Circular,
                                 SourceLocation{1, 1},
                                 "circular import of module '",
                                 pool.lookupView(path), "'");
                    return nullptr;
                }

                // ─── Load source ──────────────────────────────────────────
                if (!options.loadModule)
                {
                    diag.errorAt(DiagCode::Import_ModuleNotFound,
                                 SourceLocation{1, 1},
                                 "no module loader configured; cannot load '",
                                 pool.lookupView(path), "'");
                    return nullptr;
                }

                const auto sourceOpt = options.loadModule(pool.lookupView(path));
                if (!sourceOpt.has_value())
                {
                    diag.errorAt(DiagCode::Import_ModuleNotFound,
                                 SourceLocation{1, 1},
                                 "cannot load module '",
                                 pool.lookupView(path), "'");
                    return nullptr;
                }

                const std::string &source = sourceOpt.value();

                // ─── Parse ────────────────────────────────────────────────
                parser::TokenStream dummyStream(std::vector<Token>{
                    Token{TokenType::EOF_TOKEN, InternedString{},
                          SourceLocation{1, 1}}});
                parser::ParserContext ctx(pool, arena, diag, dummyStream);
                ModuleAST *module = parseFile(
                    pool.lookupView(path), source, ctx);

                if (module == nullptr)
                    return nullptr;

                cache[path] = module;

                // ─── Recurse ──────────────────────────────────────────────
                inProgress.insert(path);
                for (DeclAST *decl : module->decls)
                {
                    if (decl == nullptr)
                        continue;
                    if (decl->kind != ASTKind::ImportDecl)
                        continue;

                    auto *importDecl = decl->as<ImportDeclAST>();
                    load(importDecl->path);
                }
                inProgress.erase(path);

                // ─── Add to order (post-order) ────────────────────────────
                order.push_back(module);
                return module;
            }
        };

        /// After the loader's traversal, verify that the import graph is a
        /// DAG. If a cycle exists, report Import_Circular.
        void detectModuleCycles(const std::vector<ModuleAST *> &modules,
                                StringPool &pool,
                                DiagnosticEngine &diag)
        {
            // ─── Build the graph ───────────────────────────────────────────────────
            // Node index: the module's position in `modules`.
            std::unordered_map<InternedString, size_t> indexByPath;
            for (size_t i = 0; i < modules.size(); ++i)
            {
                indexByPath[modules[i]->filePath] = i;
            }

            const size_t n = modules.size();
            std::vector<std::vector<size_t>> deps(n); // out-edges (module -> its imports)
            std::vector<int> indegree(n, 0);          // number of importers

            for (size_t i = 0; i < n; ++i)
            {
                for (DeclAST *decl : modules[i]->decls)
                {
                    if (decl == nullptr)
                        continue;
                    if (decl->kind != ASTKind::ImportDecl)
                        continue;

                    auto *importDecl = decl->as<ImportDeclAST>();
                    auto it = indexByPath.find(importDecl->path);
                    if (it == indexByPath.end())
                        continue;

                    const size_t j = it->second;
                    // The module imports `j`. Edge: `j` is a dependency of `i`.
                    // For Kahn's algorithm on "dependencies first", we want
                    // edges from dependency to dependent, and in-degree counts
                    // "number of dependencies".
                    //
                    // Let me reverse the edge direction: from j (dependency)
                    // to i (dependent). Then in-degree of i is its number of
                    // dependencies.
                    deps[j].push_back(i);
                    indegree[i]++;
                }
            }

            // ─── Kahn's algorithm ──────────────────────────────────────────────────
            std::vector<size_t> queue;
            for (size_t i = 0; i < n; ++i)
            {
                if (indegree[i] == 0)
                    queue.push_back(i);
            }

            size_t processed = 0;
            while (!queue.empty())
            {
                const size_t v = queue.back();
                queue.pop_back();
                ++processed;

                for (size_t dependent : deps[v])
                {
                    if (--indegree[dependent] == 0)
                    {
                        queue.push_back(dependent);
                    }
                }
            }

            // ─── Report a cycle ────────────────────────────────────────────────────
            if (processed != n)
            {
                // Find a module that is part of the cycle (in-degree > 0).
                for (size_t i = 0; i < n; ++i)
                {
                    if (indegree[i] > 0)
                    {
                        diag.errorAt(DiagCode::Import_Circular,
                                     SourceLocation{1, 1},
                                     "circular import involving module '",
                                     pool.lookupView(modules[i]->filePath), "'");
                        break;
                    }
                }
            }
        }
    } // namespace

    ModuleSet resolveImports(ModuleAST *rootModule,
                             std::string_view rootPath,
                             const CompileOptions &options,
                             StringPool &pool,
                             ASTArena &arena,
                             DiagnosticEngine &diag)
    {
        ModuleSet result;
        if (rootModule == nullptr)
            return result;

        const InternedString rootPathInterned = pool.intern(rootPath);
        Loader loader(pool, arena, diag, options);
        loader.cache[rootPathInterned] = rootModule;

        loader.inProgress.insert(rootPathInterned);
        for (DeclAST *decl : rootModule->decls)
        {
            if (decl == nullptr)
                continue;
            if (decl->kind != ASTKind::ImportDecl)
                continue;

            auto *importDecl = decl->as<ImportDeclAST>();
            loader.load(importDecl->path);
        }
        loader.inProgress.erase(rootPathInterned);

        result.modules = std::move(loader.order);
        result.modules.push_back(rootModule);

        // Verify the import graph is acyclic.
        detectModuleCycles(result.modules, pool, diag);

        return result;
    }

} // namespace lucid::sema