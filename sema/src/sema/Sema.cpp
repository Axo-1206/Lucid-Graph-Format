/// @file sema/src/sema/Sema.cpp
///
/// @brief Implementation of Sema's public entry points and the
///        multi-module pipeline.

#include "sema/Sema.hpp"
#include "AttributeChecker.hpp"
#include "DeadCodeChecker.hpp"
#include "DeclHelpers.hpp"
#include "GraphBuilder.hpp"
#include "ImportResolver.hpp"
#include "Resolver.hpp"
#include "SymbolCollector.hpp"
#include "TypeChecker.hpp"

#include "core/ast/DeclAST.hpp"
#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"
#include "sema/ModuleTable.hpp"

#include <string>
#include <unordered_map>
#include <vector>

using namespace lucid::diag;

namespace lucid::sema
{

    namespace
    {

        // ─── RAII diagnostic file guard ────────────────────────────────────────

        /// Set the diagnostic engine's current file for the scope, restoring
        /// the previous file on destruction.
        struct ScopedDiagFile
        {
            DiagnosticEngine &diag;
            InternedString previous;

            ScopedDiagFile(DiagnosticEngine &d, InternedString file)
                : diag(d), previous(d.currentFile())
            {
                diag.setCurrentFile(file);
            }

            ~ScopedDiagFile()
            {
                diag.setCurrentFile(previous);
            }

            ScopedDiagFile(const ScopedDiagFile &)             = delete;
            ScopedDiagFile &operator=(const ScopedDiagFile &)  = delete;
        };

        // ─── Module name from path ─────────────────────────────────────────────

        /// Compute a module's name from its path. The name is the last
        /// segment after the last '.' or '/'.
        ///   - `core.keys`  → `keys`
        ///   - `physics`    → `physics`
        ///   - `path/to/f`  → `f`
        InternedString moduleNameFromPath(std::string_view path,
                                          StringPool &pool)
        {
            size_t sep = std::string_view::npos;
            for (size_t i = 0; i < path.size(); ++i)
            {
                if (path[i] == '.' || path[i] == '/')
                    sep = i;
            }
            const std::string_view name =
                (sep == std::string_view::npos) ? path : path.substr(sep + 1);
            return pool.intern(name);
        }

        // ─── Export merge ──────────────────────────────────────────────────────

        /// For each module, inject the exported declarations of its imports
        /// into its own symbol table.
        void mergeExports(const ModuleSet &moduleSet,
                          std::vector<SymbolTable> &symbols,
                          StringPool &pool,
                          DiagnosticEngine &diag)
        {
            // Map from a module's path to its symbol table.
            std::unordered_map<InternedString, const SymbolTable *>
                symbolsByPath;
            for (size_t i = 0; i < moduleSet.modules.size(); ++i)
            {
                symbolsByPath[moduleSet.modules[i]->filePath] = &symbols[i];
            }

            // Map from a module's path to its AST.
            std::unordered_map<InternedString, ModuleAST *> modulesByPath;
            for (size_t i = 0; i < moduleSet.modules.size(); ++i)
            {
                modulesByPath[moduleSet.modules[i]->filePath] =
                    moduleSet.modules[i];
            }

            (void)symbolsByPath; // used implicitly through modulesByPath

            const InternedString exportName = pool.intern("export");

            for (size_t i = 0; i < moduleSet.modules.size(); ++i)
            {
                ModuleAST *M = moduleSet.modules[i];

                for (DeclAST *decl : M->decls)
                {
                    if (decl == nullptr)
                        continue;
                    if (decl->kind != ASTKind::ImportDecl)
                        continue;

                    auto *importDecl = decl->as<ImportDeclAST>();
                    auto modIt = modulesByPath.find(importDecl->path);
                    if (modIt == modulesByPath.end())
                        continue;

                    ModuleAST *imported = modIt->second;
                    for (DeclAST *importedDecl : imported->decls)
                    {
                        if (importedDecl == nullptr)
                            continue;
                        if (importedDecl->kind == ASTKind::ImportDecl)
                            continue;
                        if (!hasAttribute(importedDecl, exportName))
                            continue;

                        SymbolKind kind;
                        if (!symbolKindOf(importedDecl->kind, kind))
                            continue;
                        if (!importedDecl->name.isValid())
                            continue;

                        symbols[i].add(importedDecl->name, kind,
                                       importedDecl, diag);
                    }
                }
            }
        }

    } // namespace

    // ─── Multi-module pipeline ────────────────────────────────────────────────

    namespace
    {

        CompileResult compileModules(ModuleAST *rootModule,
                                     std::string_view rootPath,
                                     StringPool &pool,
                                     ASTArena &arena,
                                     const Registry &registry,
                                     CompileOptions options,
                                     DiagnosticEngine &diag)
        {
            CompileResult result;
            if (rootModule == nullptr)
                return result;

            // ─── Load imports ──────────────────────────────────────────────
            ModuleSet moduleSet = resolveImports(rootModule, rootPath,
                                                 options, pool, arena, diag);
            if (diag.hasErrors())
            {
                result.ok          = false;
                result.diagnostics = diag.all();
                return result;
            }

            // ─── Check attributes ──────────────────────────────────────────
            for (ModuleAST *m : moduleSet.modules)
            {
                ScopedDiagFile guard(diag, m->filePath);
                checkAttributes(m, pool, diag);
            }
            if (diag.hasErrors())
            {
                result.ok          = false;
                result.diagnostics = diag.all();
                return result;
            }

            // ─── Pass 1: symbol collection ─────────────────────────────────
            std::vector<SymbolTable> symbols(moduleSet.modules.size());
            for (size_t i = 0; i < moduleSet.modules.size(); ++i)
            {
                ScopedDiagFile guard(diag, moduleSet.modules[i]->filePath);
                collectSymbols(moduleSet.modules[i], symbols[i], diag);
            }
            if (diag.hasErrors())
            {
                result.ok          = false;
                result.diagnostics = diag.all();
                return result;
            }

            // ─── Export merge ──────────────────────────────────────────────
            mergeExports(moduleSet, symbols, pool, diag);
            if (diag.hasErrors())
            {
                result.ok          = false;
                result.diagnostics = diag.all();
                return result;
            }

            // ─── Build the module name table ───────────────────────────────
            ModuleTable moduleTable;
            for (size_t i = 0; i < moduleSet.modules.size(); ++i)
            {
                const std::string_view path =
                    pool.lookupView(moduleSet.modules[i]->filePath);
                const InternedString name = moduleNameFromPath(path, pool);
                moduleTable.modules[name] = &symbols[i];
            }

            // ─── Pass 2: name resolution ───────────────────────────────────
            std::vector<ResolutionMap> resolutions(moduleSet.modules.size());
            for (size_t i = 0; i < moduleSet.modules.size(); ++i)
            {
                ScopedDiagFile guard(diag, moduleSet.modules[i]->filePath);
                resolveNames(moduleSet.modules[i], symbols[i], moduleTable,
                             resolutions[i], diag);
            }
            if (diag.hasErrors())
            {
                result.ok          = false;
                result.diagnostics = diag.all();
                return result;
            }

            // ─── Pass 3: type checking ─────────────────────────────────────
            std::vector<TypeMap>          types(moduleSet.modules.size());
            std::vector<ConstantValueMap> constants(moduleSet.modules.size());
            for (size_t i = 0; i < moduleSet.modules.size(); ++i)
            {
                ScopedDiagFile guard(diag, moduleSet.modules[i]->filePath);
                checkTypes(moduleSet.modules[i], symbols[i], resolutions[i],
                           registry, types[i], constants[i], diag);
            }
            if (diag.hasErrors())
            {
                result.ok          = false;
                result.diagnostics = diag.all();
                return result;
            }

            // ─── Pass 4: graph construction ────────────────────────────────
            std::vector<ModuleContext> contexts;
            contexts.reserve(moduleSet.modules.size());
            for (size_t i = 0; i < moduleSet.modules.size(); ++i)
            {
                contexts.push_back(ModuleContext{
                    moduleSet.modules[i],
                    &symbols[i],
                    &resolutions[i],
                    &constants[i],
                    &types[i],
                });
            }

            std::unique_ptr<Graph> graph =
                buildGraphFromModules(contexts, registry, pool, diag);
            if (diag.hasErrors())
            {
                result.ok          = false;
                result.diagnostics = diag.all();
                return result;
            }

            // ─── Pass 5: dead-code detection ───────────────────────────────
            for (size_t i = 0; i < moduleSet.modules.size(); ++i)
            {
                ScopedDiagFile guard(diag, moduleSet.modules[i]->filePath);
                checkDeadCode(moduleSet.modules[i], resolutions[i],
                              registry, diag);
            }

            result.ok          = true;
            result.graph       = std::move(graph);
            result.diagnostics = diag.all();
            return result;
        }

    } // namespace

    // ─── Public entry points ──────────────────────────────────────────────────

    CompileResult compile(std::string_view source,
                          std::string_view filename,
                          const Registry &registry,
                          CompileOptions options)
    {
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag(&pool);

        parser::TokenStream dummyStream(std::vector<Token>{
            Token{TokenType::EOF_TOKEN, InternedString{},
                  SourceLocation{1, 1}}});
        parser::ParserContext ctx(pool, arena, diag, dummyStream);
        ModuleAST *module = parser::parseFile(filename, source, ctx);

        if (diag.hasErrors())
        {
            CompileResult result;
            result.ok          = false;
            result.graph       = nullptr;
            result.diagnostics = diag.all();
            return result;
        }

        // Re-use the same diagnostic engine so parse warnings are not lost.
        return compileModules(module, filename, pool, arena,
                              registry, options, diag);
    }

    /// @brief Run Sema on an already-parsed module, loading any imports.
    ///
    /// `compileModule` takes an already-parsed `ModuleAST` and runs Sema on
    /// it, loading any imports it declares via `options.loadModule`. It
    /// allocates an internal `ASTArena` for the loaded modules; that arena
    /// is destroyed when `compileModule` returns. The graph is the only
    /// output and does not reference the AST.
    CompileResult compileModule(const ModuleAST *module,
                                std::string_view source,
                                std::string_view filename,
                                StringPool &pool,
                                const Registry &registry,
                                CompileOptions options)
    {
        (void)source;

        if (module == nullptr)
            return {};

        DiagnosticEngine diag(&pool);
        ASTArena arena;

        // compileModule takes a const ModuleAST*, but the multi-module
        // pipeline needs a non-const pointer (for the import resolver to
        // allocate loaded modules alongside it). The cast is safe: the
        // graph builder and passes do not mutate the AST.
        return compileModules(const_cast<ModuleAST *>(module),
                              filename, pool, arena, registry, options, diag);
    }

} // namespace lucid::sema