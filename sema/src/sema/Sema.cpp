/// @file sema/src/sema/Sema.cpp
///
/// @brief Implementation of Sema's public entry points.

#include "sema/Sema.hpp"
#include "Resolver.hpp"
#include "SymbolCollector.hpp"
#include "TypeChecker.hpp"

#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

using namespace lucid::diag;

namespace lucid::sema
{

    CompileResult compileModule(const ModuleAST *module,
                                std::string_view source,
                                std::string_view filename,
                                StringPool &pool,
                                const Registry &registry,
                                CompileOptions options)
    {
        (void)source;
        (void)filename;
        (void)options;

        CompileResult result;
        if (module == nullptr)
            return result;

        DiagnosticEngine diag(&pool);

        // ─── Pass 1: symbol collection ─────────────────────────────────────
        SymbolTable symbols;
        collectSymbols(module, symbols, diag);

        // ─── Pass 2: name resolution ───────────────────────────────────────
        ResolutionMap resolutions;
        resolveNames(module, symbols, resolutions, diag);

        // ─── Pass 3: type checking and trigger rules ───────────────────────
        TypeMap types;
        ConstantValueMap constants;
        checkTypes(module, symbols, resolutions, registry, types, constants, diag);

        // ─── Stop if any pass reported errors ──────────────────────────────
        if (diag.hasErrors())
        {
            result.ok = false;
            result.graph = nullptr;
            result.diagnostics = diag.all();
            return result;
        }

        // ─── Pass 4+ not implemented ───────────────────────────────────────
        diag.errorAt(DiagCode::Internal_NotImplemented,
                     SourceLocation{1, 1},
                     "sema: passes after type checking "
                     "are not yet implemented");

        result.ok = false;
        result.graph = nullptr;
        result.diagnostics = diag.all();
        return result;
    }

    CompileResult compile(std::string_view source,
                          std::string_view filename,
                          const Registry &registry,
                          CompileOptions options)
    {
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag(&pool);

        parser::TokenStream dummyStream(std::vector<Token>{
            Token{TokenType::EOF_TOKEN, InternedString{}, SourceLocation{1, 1}}});
        parser::ParserContext ctx(pool, arena, diag, dummyStream);
        ModuleAST *module = parser::parseFile(filename, source, ctx);

        if (diag.hasErrors())
        {
            CompileResult result;
            result.ok = false;
            result.graph = nullptr;
            result.diagnostics = diag.all();
            return result;
        }

        return compileModule(module, source, filename, pool, registry, options);
    }

} // namespace lucid::sema