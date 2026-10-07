/// @file parser/lexer/Lexer.hpp
///
/// @brief Converts one source file's text into a flat token stream,
///        with optional comment collection.
///
/// ─── Design: the lexer interns ────────────────────────────────────────────
/// Every token that carries a payload — an identifier, a keyword, a
/// literal, a punctuation spelling — stores an InternedString, not a
/// std::string. The lexer calls pool.intern() once per distinct payload
/// and stores the 4-byte handle on the token.
///
/// Comment text is interned the same way, when comments are collected.
///
/// ─── Design: comments are collected, not dropped ──────────────────────────
/// Two entry points:
///
///   - `tokenize` — the parser's entry point. Comments are skipped and
///     discarded; the returned vector contains only tokens.
///
///   - `tokenizeWithTrivia` — the formatter's entry point. Comments are
///     collected into a TriviaBuffer and returned alongside the tokens.
///
/// The two share their scanner loop. The difference is that
/// `tokenizeWithTrivia` passes a non-null `TriviaBuffer*` to the loop,
/// and the loop appends comment entries to it when it sees them.

#pragma once

#include "core/Tokens.hpp"
#include "core/SourceLocation.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/InternedString.hpp"
#include "core/memory/StringPool.hpp"
#include "core/trivia/TriviaBuffer.hpp"

#include <string_view>
#include <vector>

namespace lucid::lexer
{

    // ─────────────────────────────────────────────────────────────────────────────
    // Public API
    // ─────────────────────────────────────────────────────────────────────────────

    /// @brief The result of tokenizeWithTrivia.
    ///
    /// The tokens are the same sequence `tokenize` would produce: every
    /// token is present, the final token is EOF_TOKEN, and error tokens
    /// (UNKNOWN) appear where the lexer recovered.
    ///
    /// The trivia is the sequence of comments the lexer encountered, in
    /// source order. It is empty when there were no comments.
    struct TokenizeResult
    {
        std::vector<Token> tokens;
        trivia::TriviaBuffer trivia;
    };

    /// @brief Tokenize a source file into a flat token stream.
    ///
    /// Comments are skipped and discarded. Every token's `value` field
    /// is an InternedString owned by `pool`. The final token is always
    /// EOF_TOKEN, even on error.
    ///
    /// The lexer reports errors through `diagnostics`. It does not
    /// abort on error; an unrecoverable malformed construct produces
    /// an UNKNOWN token and lexing continues.
    ///
    /// Use this when comments are not needed. The parser uses it; the
    /// formatter uses `tokenizeWithTrivia`.
    std::vector<Token> tokenize(std::string_view source,
                                StringPool &pool,
                                lucid::diag::DiagnosticEngine &diagnostics);

    /// @brief Tokenize a source file, collecting comments into a buffer.
    ///
    /// Same token stream as `tokenize`. Additionally, every comment the
    /// lexer encounters is appended to the returned `TriviaBuffer`, in
    /// source order, with its content interned.
    ///
    /// Use this when comments are needed. The formatter uses it.
    TokenizeResult tokenizeWithTrivia(std::string_view source,
                                      StringPool &pool,
                                      lucid::diag::DiagnosticEngine &diagnostics);

    // ─────────────────────────────────────────────────────────────────────────────
    // Character classification
    // ─────────────────────────────────────────────────────────────────────────────
    // (unchanged from the current header)

    inline bool isIdentifierStart(char c) noexcept
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    inline bool isIdentifierChar(char c) noexcept
    {
        return isIdentifierStart(c) || (c >= '0' && c <= '9');
    }

    inline bool isDigit(char c) noexcept
    {
        return c >= '0' && c <= '9';
    }

    inline bool isHexDigit(char c) noexcept
    {
        return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    inline bool isBinDigit(char c) noexcept
    {
        return c == '0' || c == '1';
    }

    inline bool isOctDigit(char c) noexcept
    {
        return c >= '0' && c <= '7';
    }

} // namespace lucid::lexer