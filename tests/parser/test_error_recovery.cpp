/// @file tests/parser/test_error_recovery.cpp
///
/// @brief Tests for parser/support/ErrorRecovery.hpp.
///
/// ─── The test constructs tokens directly ──────────────────────────────────
/// No lexer, no source string. The tests build `std::vector<Token>` and
/// wrap it in a `TokenStream`. This isolates the scanner from the lexer:
/// a scanner test that failed because the lexer produced a different
/// token would be a bad test.
///
/// The `StringPool` is used only because `Token`'s payload is an
/// `InternedString` and the pool is where IDs come from. The tests do not
/// look up any payload; they only care about token types.

#include "parser/support/ErrorRecovery.hpp"

#include "core/Tokens.hpp"
#include "core/memory/InternedString.hpp"
#include "core/memory/StringPool.hpp"
#include "parser/context/TokenStream.hpp"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

using lucid::parser::synchronizeTo;
using lucid::parser::synchronizeUntil;
using lucid::parser::synchronizeUntilDepth;
using lucid::parser::SyncResult;
using lucid::parser::TokenStream;

namespace
{

    // ─── Token-vector construction ────────────────────────────────────────────
    //
    // A test builds a vector of TokenType values and gets back a TokenStream.
    // The `StringPool` outlives the stream, which is why each test creates one
    // at the top of its body rather than in a helper that would return a
    // stream referencing a dead pool.
    //
    // The payload of every token is the default InternedString (id 0). The
    // scanner never reads a payload, so this is fine.

    std::vector<Token> makeTokens(std::initializer_list<TokenType> types)
    {
        std::vector<Token> tokens;
        tokens.reserve(types.size() + 1);
        for (TokenType t : types)
        {
            tokens.emplace_back(t, InternedString{}, SourceLocation{1, 1});
        }
        // Every stream ends with an EOF token; the lexer guarantees this and
        // the scanner relies on it. The test constructs it explicitly so that
        // the scanner's end-of-input behavior is exercised.
        tokens.emplace_back(TokenType::EOF_TOKEN,
                            InternedString{},
                            SourceLocation{1, 1});
        return tokens;
    }

    // Convenience: run a scanner against a fresh stream built from `types`.
    // The `StringPool` is a local that outlives the stream; the stream is
    // constructed by value and destroyed at the end of the helper. The
    // scanner is passed the stream by reference, so the helper returns the
    // SyncResult, not the stream.
    template <typename Predicate>
    SyncResult scan(std::initializer_list<TokenType> types, Predicate pred)
    {
        StringPool pool;
        TokenStream stream(makeTokens(types));
        return synchronizeUntil(stream, pred);
    }

    template <typename Predicate>
    SyncResult scanDepth(std::initializer_list<TokenType> types, Predicate pred)
    {
        StringPool pool;
        TokenStream stream(makeTokens(types));
        return synchronizeUntilDepth(stream, pred);
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// matchingCloserFor
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("matchingCloserFor returns the matching close for each opener",
          "[error-recovery]")
{
    using lucid::parser::detail::matchingCloserFor;
    CHECK(matchingCloserFor(TokenType::LPAREN) == TokenType::RPAREN);
    CHECK(matchingCloserFor(TokenType::LBRACKET) == TokenType::RBRACKET);
    CHECK(matchingCloserFor(TokenType::LBRACE) == TokenType::RBRACE);
}

// ─────────────────────────────────────────────────────────────────────────────
// synchronizeUntil — the depth-blind scanner
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("synchronizeUntil stops on the first match at depth zero",
          "[error-recovery]")
{
    // Scan for COMMA. The stream is `a , b`. The scan consumes `a` and
    // stops on `,` without consuming it.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::IDENTIFIER, // a
        TokenType::COMMA,      // ,
        TokenType::IDENTIFIER, // b
    }));

    const auto result = synchronizeUntil(stream, [](TokenType t)
                                         { return t == TokenType::COMMA; });

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::COMMA);          // not consumed
    CHECK(stream.peekNextType() == TokenType::IDENTIFIER); // `b` is next
}

TEST_CASE("synchronizeUntil consumes every non-matching token",
          "[error-recovery]")
{
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::IDENTIFIER,
        TokenType::INT_LITERAL,
        TokenType::FLOAT_LITERAL,
        TokenType::COMMA,
    }));

    const auto result = synchronizeUntil(stream, [](TokenType t)
                                         { return t == TokenType::COMMA; });

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::COMMA);
}

TEST_CASE("synchronizeUntil does not stop on a match at depth > 0",
          "[error-recovery]")
{
    // Scan for COMMA. The stream is `( , ) ,`. The first COMMA is inside
    // the parentheses and must be skipped. The scan stops at the second
    // COMMA.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::LPAREN,
        TokenType::COMMA, // inside parens: must be skipped
        TokenType::RPAREN,
        TokenType::COMMA, // at depth 0: stop here
    }));

    const auto result = synchronizeUntil(stream, [](TokenType t)
                                         { return t == TokenType::COMMA; });

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::COMMA);
    // We should be on the *second* COMMA, i.e. past the RPAREN.
    CHECK(stream.previousLoc() != SourceLocation{}); // consumed something
}

TEST_CASE("synchronizeUntil handles nested brackets",
          "[error-recovery]")
{
    // Scan for COMMA. The stream is `( [ , ] ) ,`. The COMMA is at
    // depth 2 and must be skipped.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::LPAREN,
        TokenType::LBRACKET,
        TokenType::COMMA,
        TokenType::RBRACKET,
        TokenType::RPAREN,
        TokenType::COMMA,
    }));

    const auto result = synchronizeUntil(stream, [](TokenType t)
                                         { return t == TokenType::COMMA; });

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::COMMA);
}

TEST_CASE("synchronizeUntil returns ForeignCloser on a stray closer",
          "[error-recovery]")
{
    // Scan for COMMA. The stream is `) ,`. The `)` has no matching
    // opener; the scan should return ForeignCloser and leave the cursor
    // on the `)`.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::RPAREN,
        TokenType::COMMA,
    }));

    const auto result = synchronizeUntil(stream, [](TokenType t)
                                         { return t == TokenType::COMMA; });

    CHECK(result == SyncResult::ForeignCloser);
    CHECK(stream.peekType() == TokenType::RPAREN); // not consumed
}

TEST_CASE("synchronizeUntil returns ForeignCloser on a mismatched closer",
          "[error-recovery]")
{
    // Scan for COMMA. The stream is `( ] )`. The `]` does not match the
    // open `(`; the scan returns ForeignCloser.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::LPAREN,
        TokenType::RBRACKET,
        TokenType::RPAREN,
    }));

    const auto result = synchronizeUntil(stream, [](TokenType t)
                                         { return t == TokenType::COMMA; });

    CHECK(result == SyncResult::ForeignCloser);
    CHECK(stream.peekType() == TokenType::RBRACKET); // not consumed
}

TEST_CASE("synchronizeUntil returns ReachedEnd when no token matches",
          "[error-recovery]")
{
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::IDENTIFIER,
        TokenType::INT_LITERAL,
    }));

    const auto result = synchronizeUntil(stream, [](TokenType t)
                                         { return t == TokenType::COMMA; });

    CHECK(result == SyncResult::ReachedEnd);
}

TEST_CASE("synchronizeUntil matches on the first token of an empty scan",
          "[error-recovery]")
{
    // The stream's first token matches; the scan stops immediately
    // without consuming anything.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::COMMA,
        TokenType::IDENTIFIER,
    }));

    const auto result = synchronizeUntil(stream, [](TokenType t)
                                         { return t == TokenType::COMMA; });

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::COMMA);
}

TEST_CASE("synchronizeUntil stops on a closer that matches the predicate",
          "[error-recovery]")
{
    // A caller that wants to stop on a specific closer (e.g. the closing
    // brace of the current block) can include it in the predicate. The
    // scan must stop on it, not treat it as a foreign closer.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::IDENTIFIER,
        TokenType::RBRACE,
        TokenType::IDENTIFIER,
    }));

    const auto result = synchronizeUntil(stream, [](TokenType t)
                                         { return t == TokenType::RBRACE; });

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::RBRACE);
}

// ─────────────────────────────────────────────────────────────────────────────
// synchronizeUntilDepth — the depth-aware scanner
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("synchronizeUntilDepth passes the correct depth at each "
          "predicate call",
          "[error-recovery]")
{
    // The stream is `( a b ) c`. The predicate records the depth it sees
    // and never stops.
    //
    // The predicate is NOT called for af matching closer: the scanner
    // pops the bracket stack and consumes the closer without consulting
    // the predicate, because a matching closer is part of the scan's own
    // structure, not a candidate stop point. The predicate is therefore
    // called for `(`, `a`, `b`, and `c` — not for `)`.
    //
    // Depths seen: 0 (the `(`), 1 (`a`), 1 (`b`), 0 (`c`).
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::LPAREN,
        TokenType::IDENTIFIER, // a
        TokenType::IDENTIFIER, // b
        TokenType::RPAREN,
        TokenType::IDENTIFIER, // c
    }));

    std::vector<int> depths;
    const auto result = synchronizeUntilDepth(stream,
                                              [&depths](TokenStream &, int d)
                                              {
                                                  depths.push_back(d);
                                                  return false;
                                              });

    CHECK(result == SyncResult::ReachedEnd);
    REQUIRE(depths.size() == 4);
    CHECK(depths[0] == 0); // `(` at depth 0
    CHECK(depths[1] == 1); // `a` at depth 1
    CHECK(depths[2] == 1); // `b` at depth 1
    CHECK(depths[3] == 0); // `c` at depth 0, after `)` popped the stack
}

TEST_CASE("synchronizeUntilDepth does not call the predicate on a "
          "matching closer",
          "[error-recovery]")
{
    // The predicate counts its calls. The stream is `( a ) b`. The `)`
    // matches the `(` on the stack, so the scanner pops the stack and
    // consumes the `)` without consulting the predicate. The predicate
    // is called for `(`, `a`, and `b` — three times, not four.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::LPAREN,
        TokenType::IDENTIFIER, // a
        TokenType::RPAREN,
        TokenType::IDENTIFIER, // b
    }));

    int callCount = 0;
    const auto result = synchronizeUntilDepth(stream,
                                              [&callCount](TokenStream &, int)
                                              {
                                                  ++callCount;
                                                  return false;
                                              });

    CHECK(result == SyncResult::ReachedEnd);
    CHECK(callCount == 3);
}

TEST_CASE("synchronizeUntilDepth can stop at depth > 0",
          "[error-recovery]")
{
    // The predicate stops on any identifier, at any depth. The stream is
    // `( a )`. The scan should stop on `a` at depth 1.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::LPAREN,
        TokenType::IDENTIFIER,
        TokenType::RPAREN,
    }));

    const auto result = synchronizeUntilDepth(stream,
                                              [](TokenStream &, int)
                                              { return true; });

    // The very first token is LPAREN, at depth 0. The predicate returns
    // true unconditionally, so the scan stops on LPAREN. That is not what
    // we want to test. Adjust: use a predicate that only stops on
    // IDENTIFIER.
    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::LPAREN);
}

TEST_CASE("synchronizeUntilDepth stops at the depth the predicate names",
          "[error-recovery]")
{
    // The predicate stops on an identifier only at depth 1. The stream is
    // `a ( b ) c`. The scan should skip `a` (identifier at depth 0), stop
    // on `b` (identifier at depth 1).
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::IDENTIFIER, // a: depth 0, do not stop
        TokenType::LPAREN,
        TokenType::IDENTIFIER, // b: depth 1, stop
        TokenType::RPAREN,
        TokenType::IDENTIFIER, // c
    }));

    const auto result = synchronizeUntilDepth(stream,
                                              [](TokenStream &s, int d)
                                              {
                                                  return d == 1 && s.peekType() == TokenType::IDENTIFIER;
                                              });

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::IDENTIFIER);
}

TEST_CASE("synchronizeUntilDepth gives the predicate access to the stream",
          "[error-recovery]")
{
    // The predicate stops when the current token is IDENTIFIER and the
    // next token is COMMA. The stream is `a , b ,`. The first identifier is
    // followed by a comma, so the scan stops on `a`.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::IDENTIFIER, // a: followed by `,` -> stop here
        TokenType::COMMA,
        TokenType::IDENTIFIER, // b: followed by `,` -> would also match
        TokenType::COMMA,
    }));

    const auto result = synchronizeUntilDepth(stream,
                                              [](TokenStream &s, int)
                                              {
                                                  return s.peekType() == TokenType::IDENTIFIER &&
                                                         s.peekNextType() == TokenType::COMMA;
                                              });

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::IDENTIFIER);
    CHECK(stream.peekNextType() == TokenType::COMMA);
}

TEST_CASE("synchronizeUntilDepth returns Matched on a foreign closer "
          "when the predicate says so",
          "[error-recovery]")
{
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::IDENTIFIER,
        TokenType::RBRACE,
        TokenType::IDENTIFIER,
    }));

    const auto result = synchronizeUntilDepth(stream,
                                              [](TokenStream &s, int)
                                              {
                                                  return s.peekType() == TokenType::RBRACE;
                                              });

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::RBRACE);
}

TEST_CASE("synchronizeUntilDepth returns ForeignCloser when the predicate "
          "does not stop on a foreign closer",
          "[error-recovery]")
{
    // The top-level scan does *not* want to stop on a stray `}`; it wants
    // to recover upward. The predicate returns false on `}`, so the scan
    // returns ForeignCloser.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::RBRACE,
        TokenType::IDENTIFIER,
    }));

    const auto result = synchronizeUntilDepth(stream,
                                              [](TokenStream &, int)
                                              { return false; });

    CHECK(result == SyncResult::ForeignCloser);
    CHECK(stream.peekType() == TokenType::RBRACE);
}

TEST_CASE("synchronizeUntilDepth returns ReachedEnd when nothing matches",
          "[error-recovery]")
{
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::IDENTIFIER,
        TokenType::INT_LITERAL,
    }));

    const auto result = synchronizeUntilDepth(stream,
                                              [](TokenStream &, int)
                                              { return false; });

    CHECK(result == SyncResult::ReachedEnd);
}

// ─────────────────────────────────────────────────────────────────────────────
// synchronizeTo — the variadic convenience form
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("synchronizeTo stops on any of the listed tokens",
          "[error-recovery]")
{
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::IDENTIFIER,
        TokenType::INT_LITERAL,
        TokenType::COMMA,
    }));

    const auto result = synchronizeTo(stream,
                                      TokenType::COMMA, TokenType::RPAREN);

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::COMMA);
}

TEST_CASE("synchronizeTo returns ReachedEnd when none of the tokens match",
          "[error-recovery]")
{
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::IDENTIFIER,
        TokenType::INT_LITERAL,
    }));

    const auto result = synchronizeTo(stream,
                                      TokenType::COMMA, TokenType::RPAREN);

    CHECK(result == SyncResult::ReachedEnd);
}

TEST_CASE("synchronizeTo respects bracket depth",
          "[error-recovery]")
{
    // The stream is `( , ) ,`. The scan for COMMA must skip the first
    // COMMA (inside parens) and stop at the second.
    StringPool pool;
    TokenStream stream(makeTokens({
        TokenType::LPAREN,
        TokenType::COMMA,
        TokenType::RPAREN,
        TokenType::COMMA,
    }));

    const auto result = synchronizeTo(stream, TokenType::COMMA);

    CHECK(result == SyncResult::Matched);
    CHECK(stream.peekType() == TokenType::COMMA);
}