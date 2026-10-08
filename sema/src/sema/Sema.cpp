/// @file sema/src/sema/Sema.cpp
///
/// @brief Implementation of Sema's public entry points. STUB for Pass 1.
///
/// ─── Current state ────────────────────────────────────────────────────────
/// `compileModule` runs Pass 1 (symbol collection) and then stops,
/// reporting an Internal_NotImplemented diagnostic that says the
/// remaining passes are not yet implemented.
///
/// `compile` lexes, parses, and delegates to `compileModule`.
///
/// Step 7.3 replaces the "stop after Pass 1" behavior with the resolver
/// walk. Step 7.4 adds type checking, and so on. Each step adds a pass
/// and removes the "not implemented" diagnostic.

#include "sema/Sema.hpp"
#include "SymbolCollector.hpp"

#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

using namespace lucid::diag;

namespace lucid::sema
{

    // =========================================================================
    // compileModule
    // =========================================================================

    CompileResult compileModule(const ModuleAST *module,
                                std::string_view source,
                                std::string_view filename,
                                StringPool &pool,
                                const Registry &registry,
                                CompileOptions options)
    {
        (void)source;   // unused until later passes need it
        (void)filename; // unused until later passes tag diagnostics
        (void)registry; // unused until type checking
        (void)options;  // unused until import loading

        CompileResult result;

        if (module == nullptr)
        {
            return result;
        }

        // ─── A diagnostic engine for this compile ─────────────────────────
        DiagnosticEngine diag(&pool);

        // ─── Pass 1: symbol collection ─────────────────────────────────────
        SymbolTable symbols;
        collectSymbols(module, symbols, diag);

        // ─── Passes 2+ are not yet implemented ─────────────────────────────
        diag.errorAt(DiagCode::Internal_NotImplemented,
                     SourceLocation{1, 1},
                     "sema: passes after symbol collection "
                     "are not yet implemented");

        result.ok = false;
        result.graph = nullptr;
        result.diagnostics = diag.all();
        return result;
    }

    // =========================================================================
    // compile
    // =========================================================================

    CompileResult compile(std::string_view source,
                          std::string_view filename,
                          const Registry &registry,
                          CompileOptions options)
    {
        // ─── A session for this compile ────────────────────────────────────
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag(&pool);

        // ─── Parse ─────────────────────────────────────────────────────────
        parser::TokenStream dummyStream(std::vector<Token>{
            Token{TokenType::EOF_TOKEN, InternedString{}, SourceLocation{1, 1}}});
        parser::ParserContext ctx(pool, arena, diag, dummyStream);
        ModuleAST *module = parser::parseFile(filename, source, ctx);

        // ─── Parse errors: return early ────────────────────────────────────
        if (diag.hasErrors())
        {
            CompileResult result;
            result.ok = false;
            result.graph = nullptr;
            result.diagnostics = diag.all();
            return result;
        }

        // ─── Delegate to compileModule ─────────────────────────────────────
        return compileModule(module, source, filename, pool, registry, options);
    }

} // namespace lucid::sema
