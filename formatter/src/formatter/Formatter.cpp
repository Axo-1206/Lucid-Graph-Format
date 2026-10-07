/// @file formatter/src/formatter/Formatter.cpp
///
/// @brief Implementation of the formatter's entry points.

#include "formatter/Formatter.hpp"
#include "LayoutBuilder.hpp"

#include "core/diagnostics/DiagCode.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/trivia/TriviaBuffer.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"
#include "parser/lexer/Lexer.hpp"

#include <utility>
#include <vector>

using namespace lucid::diag;

namespace lucid::formatter
{

    // =========================================================================
    // formatModule — the lower-level entry point
    // =========================================================================

    std::string formatModule(const ModuleAST *module,
                             std::string_view source,
                             StringPool &pool,
                             FormatOptions options)
    {
        if (!module)
        {
            return {};
        }

        // Re-lex the source to collect comments. The parser discarded
        // them; the formatter needs them.
        //
        // We use a fresh DiagnosticEngine because the trivia collection
        // is not allowed to report errors. The lexer's contract is to
        // report only on genuinely malformed input; a source that
        // parsed cleanly has no lex errors either. So the engine stays
        // empty.
        DiagnosticEngine triviaDiags;
        lexer::TokenizeResult lexResult =
            lexer::tokenizeWithTrivia(source, const_cast<StringPool &>(pool),
                                      triviaDiags);

        LayoutBuilder builder(pool, lexResult.trivia, options);
        return builder.build(module);
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
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag(&pool);

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
        result.text = formatModule(module, source, pool, options);
        result.ok = true;
        result.diagnostics = diag.all(); // empty, since parse succeeded
        return result;
    }

} // namespace lucid::formatter