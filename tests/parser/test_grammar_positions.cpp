/// @file tests/parser/test_grammar_positions.cpp
///
/// @brief Tests for parser/support/GrammarPositions.hpp.
///
/// ─── What these tests check ───────────────────────────────────────────────
/// Each predicate is tested with a table that lists *every* TokenType and
/// its expected answer. The tables are exhaustive: every enumerator of
/// TokenType appears exactly once per table. If a new TokenType is added
/// to Tokens.hpp and not added to these tables, the switch in the test
/// helper warns at compile time (the helper has no default case, and a
/// missing enumerator in a switch over an enum class without a default
/// produces a -Wswitch warning, which we promote to an error in tests).
///
/// That is deliberate. The start sets are grammar facts; when the
/// vocabulary grows, the start sets must be revisited. A compile error
/// is a better alarm than a runtime surprise.

#include "parser/support/GrammarPositions.hpp"

#include "core/Tokens.hpp"

#include <catch2/catch_test_macros.hpp>

using lucid::parser::canStartTopDecl;
using lucid::parser::canStartType;
using lucid::parser::canStartValue;

namespace
{

    // ─── Exhaustive coverage helpers ──────────────────────────────────────────
    //
    // Each helper has a switch over every TokenType and no default case. If a
    // new enumerator is added to TokenType, the switch no longer covers it,
    // and the compiler (with -Wswitch, which the test target enables) reports
    // it. The test file therefore fails to compile until the tables below are
    // updated. That is the intent.

    constexpr bool topDeclExpected(TokenType t) noexcept
    {
        switch (t)
        {
        case TokenType::EOF_TOKEN:
            return false;
        case TokenType::UNKNOWN:
            return false;
        case TokenType::IDENTIFIER:
            return false;
        case TokenType::KW_IMPORT:
            return true;
        case TokenType::KW_FROM:
            return false;
        case TokenType::KW_ENUM:
            return true;
        case TokenType::KW_RESOURCE:
            return true;
        case TokenType::KW_NODE:
            return true;
        case TokenType::KW_ON:
            return false;
        case TokenType::INT_LITERAL:
            return false;
        case TokenType::FLOAT_LITERAL:
            return false;
        case TokenType::STRING_LITERAL:
            return false;
        case TokenType::CHAR_LITERAL:
            return false;
        case TokenType::BOOL_LITERAL:
            return false;
        case TokenType::NIL_LITERAL:
            return false;
        case TokenType::LPAREN:
            return false;
        case TokenType::RPAREN:
            return false;
        case TokenType::LBRACE:
            return false;
        case TokenType::RBRACE:
            return false;
        case TokenType::LBRACKET:
            return false;
        case TokenType::RBRACKET:
            return false;
        case TokenType::COMMA:
            return false;
        case TokenType::DOT:
            return false;
        case TokenType::COLON:
            return false;
        case TokenType::COLON_COLON:
            return false;
        case TokenType::EQUALS:
            return false;
        case TokenType::AT_SIGN:
            return true;
        }
        return false;
    }

    constexpr bool valueExpected(TokenType t) noexcept
    {
        switch (t)
        {
        case TokenType::EOF_TOKEN:
            return false;
        case TokenType::UNKNOWN:
            return false;
        case TokenType::IDENTIFIER:
            return true;
        case TokenType::KW_IMPORT:
            return false;
        case TokenType::KW_FROM:
            return false;
        case TokenType::KW_ENUM:
            return false;
        case TokenType::KW_RESOURCE:
            return false;
        case TokenType::KW_NODE:
            return false;
        case TokenType::KW_ON:
            return false;
        case TokenType::INT_LITERAL:
            return true;
        case TokenType::FLOAT_LITERAL:
            return true;
        case TokenType::STRING_LITERAL:
            return true;
        case TokenType::CHAR_LITERAL:
            return true;
        case TokenType::BOOL_LITERAL:
            return true;
        case TokenType::NIL_LITERAL:
            return true;
        case TokenType::LPAREN:
            return false;
        case TokenType::RPAREN:
            return false;
        case TokenType::LBRACE:
            return false;
        case TokenType::RBRACE:
            return false;
        case TokenType::LBRACKET:
            return false;
        case TokenType::RBRACKET:
            return false;
        case TokenType::COMMA:
            return false;
        case TokenType::DOT:
            return false;
        case TokenType::COLON:
            return false;
        case TokenType::COLON_COLON:
            return false;
        case TokenType::EQUALS:
            return false;
        case TokenType::AT_SIGN:
            return false;
        }
        return false;
    }

    constexpr bool typeExpected(TokenType t) noexcept
    {
        switch (t)
        {
        case TokenType::EOF_TOKEN:
            return false;
        case TokenType::UNKNOWN:
            return false;
        case TokenType::IDENTIFIER:
            return true;
        case TokenType::KW_IMPORT:
            return false;
        case TokenType::KW_FROM:
            return false;
        case TokenType::KW_ENUM:
            return false;
        case TokenType::KW_RESOURCE:
            return false;
        case TokenType::KW_NODE:
            return false;
        case TokenType::KW_ON:
            return false;
        case TokenType::INT_LITERAL:
            return false;
        case TokenType::FLOAT_LITERAL:
            return false;
        case TokenType::STRING_LITERAL:
            return false;
        case TokenType::CHAR_LITERAL:
            return false;
        case TokenType::BOOL_LITERAL:
            return false;
        case TokenType::NIL_LITERAL:
            return false;
        case TokenType::LPAREN:
            return false;
        case TokenType::RPAREN:
            return false;
        case TokenType::LBRACE:
            return false;
        case TokenType::RBRACE:
            return false;
        case TokenType::LBRACKET:
            return false;
        case TokenType::RBRACKET:
            return false;
        case TokenType::COMMA:
            return false;
        case TokenType::DOT:
            return false;
        case TokenType::COLON:
            return false;
        case TokenType::COLON_COLON:
            return false;
        case TokenType::EQUALS:
            return false;
        case TokenType::AT_SIGN:
            return false;
        }
        return false;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// The four predicates, tested against every TokenType
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("canStartTopDecl matches the grammar's top_decl start set",
          "[grammar-positions]")
{
    for (int i = 0; i <= static_cast<int>(TokenType::AT_SIGN); ++i)
    {
        const auto t = static_cast<TokenType>(i);
        INFO("TokenType value: " << i);
        CHECK(canStartTopDecl(t) == topDeclExpected(t));
    }
}

TEST_CASE("canStartValue matches the grammar's value start set",
          "[grammar-positions]")
{
    for (int i = 0; i <= static_cast<int>(TokenType::AT_SIGN); ++i)
    {
        const auto t = static_cast<TokenType>(i);
        INFO("TokenType value: " << i);
        CHECK(canStartValue(t) == valueExpected(t));
    }
}

TEST_CASE("canStartType matches the grammar's type_id start set",
          "[grammar-positions]")
{
    for (int i = 0; i <= static_cast<int>(TokenType::AT_SIGN); ++i)
    {
        const auto t = static_cast<TokenType>(i);
        INFO("TokenType value: " << i);
        CHECK(canStartType(t) == typeExpected(t));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Named cases: a handful of high-signal spot checks
// ─────────────────────────────────────────────────────────────────────────────
//
// The exhaustive tables above are the real test. These are the cases a
// reader wants to see stated by name when they open the file.

TEST_CASE("IDENTIFIER begins a value and a type, but not a declaration",
          "[grammar-positions]")
{
    CHECK(canStartValue(TokenType::IDENTIFIER));
    CHECK(canStartType(TokenType::IDENTIFIER));
    CHECK_FALSE(canStartTopDecl(TokenType::IDENTIFIER));
}

TEST_CASE("Every literal token begins a value and nothing else",
          "[grammar-positions]")
{
    const TokenType literals[] = {
        TokenType::INT_LITERAL,
        TokenType::FLOAT_LITERAL,
        TokenType::STRING_LITERAL,
        TokenType::CHAR_LITERAL,
        TokenType::BOOL_LITERAL,
        TokenType::NIL_LITERAL,
    };
    for (TokenType t : literals)
    {
        INFO("literal token: " << tokenTypeName(t));
        CHECK(canStartValue(t));
        CHECK_FALSE(canStartType(t));
        CHECK_FALSE(canStartTopDecl(t));
    }
}

TEST_CASE("KW_FROM begins nothing",
          "[grammar-positions]")
{
    CHECK_FALSE(canStartTopDecl(TokenType::KW_FROM));
    CHECK_FALSE(canStartValue(TokenType::KW_FROM));
    CHECK_FALSE(canStartType(TokenType::KW_FROM));
}

TEST_CASE("EOF_TOKEN begins nothing",
          "[grammar-positions]")
{
    CHECK_FALSE(canStartTopDecl(TokenType::EOF_TOKEN));
    CHECK_FALSE(canStartValue(TokenType::EOF_TOKEN));
    CHECK_FALSE(canStartType(TokenType::EOF_TOKEN));
}

TEST_CASE("COLON and COLON_COLON begin nothing",
          "[grammar-positions]")
{
    // The field separator and the module qualifier are never the first
    // token of a declaration, a value, or a type. A type starts with an
    // identifier; the qualifier is optional and appears after it.
    CHECK_FALSE(canStartTopDecl(TokenType::COLON));
    CHECK_FALSE(canStartValue(TokenType::COLON));
    CHECK_FALSE(canStartType(TokenType::COLON));

    CHECK_FALSE(canStartTopDecl(TokenType::COLON_COLON));
    CHECK_FALSE(canStartValue(TokenType::COLON_COLON));
    CHECK_FALSE(canStartType(TokenType::COLON_COLON));
}

TEST_CASE("AT_SIGN begins a top-level declaration only",
          "[grammar-positions]")
{
    // An attribute list may precede any of the four top-level
    // declarations. It does not begin a value or a type.
    CHECK(canStartTopDecl(TokenType::AT_SIGN));
    CHECK_FALSE(canStartValue(TokenType::AT_SIGN));
    CHECK_FALSE(canStartType(TokenType::AT_SIGN));
}