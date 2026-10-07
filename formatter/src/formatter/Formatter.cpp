/// @file formatter/src/formatter/Formatter.cpp
///
/// @brief Implementation of the formatter's entry points.
///
/// ─── Current state ────────────────────────────────────────────────────────
/// `formatModule` is a stub. It returns an empty string. The real
/// implementation lands in Step 5.4 with the LayoutBuilder.
///
/// `format` is fully implemented. It creates a session, lexes, parses,
/// and calls `formatModule`. Because `formatModule` is a stub, `format`
/// currently appends an `Internal_NotImplemented` diagnostic and returns
/// `ok = false` for any input that parses cleanly. This temporary block
/// is removed in Step 5.4.

#include "formatter/Formatter.hpp"

#include "core/diagnostics/DiagCode.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/trivia/TriviaBuffer.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

#include <utility>
#include <vector>

using namespace lucid::diag;

namespace lucid::formatter
{

    // =========================================================================
    // formatModule — the lower-level entry point. STUB.
    // =========================================================================

    std::string formatModule(const ModuleAST *module,
                             std::string_view source,
                             const StringPool &pool,
                             FormatOptions options)
    {
        // The stub. Step 5.4 replaces this with the LayoutBuilder.
        (void)module;
        (void)source;
        (void)pool;
        (void)options;
        return {};
    }

    // =========================================================================
    // format — the public entry point
    // =========================================================================

    FormatResult format(std::string_view source,
                        std::string_view filename,
                        FormatOptions options)
    {
        FormatResult result;

        // ─── The session ───────────────────────────────────────────────────
        // One session per call. The pool, arena, and diags are local;
        // they die when the function returns. The AST lives in the
        // arena, so the formatted text is copied out before the arena
        // is destroyed — which happens when the function returns.
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag(&pool);

        // ─── Lex and parse ─────────────────────────────────────────────────
        // The parser's entry point internally lexes. We do not need to
        // lex here; the parser does it. But we do need the trivia buffer
        // for the formatter, and the parser discards it.
        //
        // For now, we let the parser lex and parse. The trivia is not
        // collected in this function. In Step 5.4, formatModule will
        // re-lex the source to collect trivia. The double lex is
        // acceptable: the parser's lex is fast, and the alternative —
        // threading the token stream through the parser — is a larger
        // change for no benefit yet.
        //
        // The parse errors, if any, go into `diag`.
        parser::TokenStream dummyStream(std::vector<Token>{
            Token{TokenType::EOF_TOKEN, InternedString{}, SourceLocation{1, 1}}});
        parser::ParserContext ctx(pool, arena, diag, dummyStream);
        ModuleAST *module = parser::parseFile(filename, source, ctx);

        // ─── Parse errors: return early ────────────────────────────────────
        if (diag.hasErrors())
        {
            result.ok = false;
            result.text = {};
            result.diagnostics = diag.all();
            return result;
        }

        // ─── Format ────────────────────────────────────────────────────────
        // The module is parsed cleanly. Call formatModule.
        result.text = formatModule(module, source, pool, options);

        // ─── TEMPORARY (Step 5.3 only) ─────────────────────────────────────
        // formatModule is a stub. Until Step 5.4 implements it, report a
        // not-implemented diagnostic so a caller cannot mistake an empty
        // string for a real formatting result. This block is removed in
        // Step 5.4.
        result.diagnostics = diag.all();
        result.diagnostics.push_back(Diagnostic{
            Severity::Error,
            DiagCode::Internal_NotImplemented,
            SourceLocation{1, 1},
            "formatter: formatModule not yet implemented",
            InternedString{},
        });
        result.ok = false;
        return result;
    }

} // namespace lucid::formatter