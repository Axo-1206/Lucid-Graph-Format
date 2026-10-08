/// @file tests/core/test_tokens.cpp
///
/// @brief Tests for the token vocabulary: TokenType, LiteralKind, the
///        classification predicates, and the name functions.

#include "core/Tokens.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

// ─────────────────────────────────────────────────────────────────────────────
// TokenType basics
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TokenType: EOF is the zero value", "[core][tokens]")
{
    REQUIRE(static_cast<uint16_t>(TokenType::EOF_TOKEN) == 0);
}

TEST_CASE("TokenType: every keyword is contiguous", "[core][tokens]")
{
    // The keyword block runs from KW_IMPORT through KW_ON. This test
    // documents the block layout; the isKeyword predicate uses the same
    // range, and the range is what makes the predicate cheap.
    REQUIRE(static_cast<uint16_t>(TokenType::KW_IMPORT) <=
            static_cast<uint16_t>(TokenType::KW_ON));
}

TEST_CASE("TokenType: every literal is contiguous", "[core][tokens]")
{
    REQUIRE(static_cast<uint16_t>(TokenType::INT_LITERAL) <=
            static_cast<uint16_t>(TokenType::NIL_LITERAL));
}

TEST_CASE("TokenType: punctuation is a contiguous block", "[core][tokens]")
{
    // The punctuation tokens run from LPAREN through AT_SIGN. Unlike
    // isKeyword and isLiteral, isPunctuation uses an explicit switch, so
    // this range is documentation, not a predicate. The test still holds:
    // every punctuation token is in the block, and nothing else is.
    REQUIRE(static_cast<uint16_t>(TokenType::LPAREN) <=
            static_cast<uint16_t>(TokenType::AT_SIGN));
}

// ─────────────────────────────────────────────────────────────────────────────
// isKeyword
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("isKeyword: true for the six keywords", "[core][tokens]")
{
    REQUIRE(isKeyword(TokenType::KW_IMPORT));
    REQUIRE(isKeyword(TokenType::KW_FROM));
    REQUIRE(isKeyword(TokenType::KW_ENUM));
    REQUIRE(isKeyword(TokenType::KW_RESOURCE));
    REQUIRE(isKeyword(TokenType::KW_NODE));
    REQUIRE(isKeyword(TokenType::KW_ON));
}

TEST_CASE("isKeyword: false for non-keywords", "[core][tokens]")
{
    REQUIRE_FALSE(isKeyword(TokenType::IDENTIFIER));
    REQUIRE_FALSE(isKeyword(TokenType::INT_LITERAL));
    REQUIRE_FALSE(isKeyword(TokenType::LPAREN));
    REQUIRE_FALSE(isKeyword(TokenType::COLON_COLON));
    REQUIRE_FALSE(isKeyword(TokenType::EOF_TOKEN));
    REQUIRE_FALSE(isKeyword(TokenType::UNKNOWN));
}

// ─────────────────────────────────────────────────────────────────────────────
// isDeclarationKeyword
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("isDeclarationKeyword: true for the four declaration keywords",
          "[core][tokens]")
{
    REQUIRE(isDeclarationKeyword(TokenType::KW_IMPORT));
    REQUIRE(isDeclarationKeyword(TokenType::KW_ENUM));
    REQUIRE(isDeclarationKeyword(TokenType::KW_RESOURCE));
    REQUIRE(isDeclarationKeyword(TokenType::KW_NODE));
}

TEST_CASE("isDeclarationKeyword: false for other keywords",
          "[core][tokens]")
{
    REQUIRE_FALSE(isDeclarationKeyword(TokenType::KW_FROM));
    REQUIRE_FALSE(isDeclarationKeyword(TokenType::KW_ON));
}

// ─────────────────────────────────────────────────────────────────────────────
// isLiteral
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("isLiteral: true for the six literal forms", "[core][tokens]")
{
    REQUIRE(isLiteral(TokenType::INT_LITERAL));
    REQUIRE(isLiteral(TokenType::FLOAT_LITERAL));
    REQUIRE(isLiteral(TokenType::STRING_LITERAL));
    REQUIRE(isLiteral(TokenType::CHAR_LITERAL));
    REQUIRE(isLiteral(TokenType::BOOL_LITERAL));
    REQUIRE(isLiteral(TokenType::NIL_LITERAL));
}

TEST_CASE("isLiteral: false for identifier and keywords", "[core][tokens]")
{
    REQUIRE_FALSE(isLiteral(TokenType::IDENTIFIER));
    REQUIRE_FALSE(isLiteral(TokenType::KW_NODE));
    REQUIRE_FALSE(isLiteral(TokenType::LPAREN));
}

// ─────────────────────────────────────────────────────────────────────────────
// isPunctuation and delimiter helpers
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("isPunctuation: true for the twelve punctuation marks",
          "[core][tokens]")
{
    REQUIRE(isPunctuation(TokenType::LPAREN));      // (
    REQUIRE(isPunctuation(TokenType::RPAREN));      // )
    REQUIRE(isPunctuation(TokenType::LBRACE));      // {
    REQUIRE(isPunctuation(TokenType::RBRACE));      // }
    REQUIRE(isPunctuation(TokenType::LBRACKET));    // [  reserved
    REQUIRE(isPunctuation(TokenType::RBRACKET));    // ]  reserved
    REQUIRE(isPunctuation(TokenType::COMMA));       // ,
    REQUIRE(isPunctuation(TokenType::DOT));         // .
    REQUIRE(isPunctuation(TokenType::COLON));       // :
    REQUIRE(isPunctuation(TokenType::COLON_COLON)); // ::
    REQUIRE(isPunctuation(TokenType::EQUALS));      // =
    REQUIRE(isPunctuation(TokenType::AT_SIGN));     // @
}

TEST_CASE("isPunctuation: false for identifier and literals",
          "[core][tokens]")
{
    REQUIRE_FALSE(isPunctuation(TokenType::IDENTIFIER));
    REQUIRE_FALSE(isPunctuation(TokenType::INT_LITERAL));
    REQUIRE_FALSE(isPunctuation(TokenType::KW_NODE));
    REQUIRE_FALSE(isPunctuation(TokenType::EOF_TOKEN));
    REQUIRE_FALSE(isPunctuation(TokenType::UNKNOWN));
}

TEST_CASE("isOpeningDelimiter and isClosingDelimiter",
          "[core][tokens]")
{
    REQUIRE(isOpeningDelimiter(TokenType::LPAREN));
    REQUIRE(isOpeningDelimiter(TokenType::LBRACE));
    REQUIRE(isOpeningDelimiter(TokenType::LBRACKET));

    REQUIRE(isClosingDelimiter(TokenType::RPAREN));
    REQUIRE(isClosingDelimiter(TokenType::RBRACE));
    REQUIRE(isClosingDelimiter(TokenType::RBRACKET));

    REQUIRE_FALSE(isOpeningDelimiter(TokenType::RPAREN));
    REQUIRE_FALSE(isClosingDelimiter(TokenType::LPAREN));

    REQUIRE_FALSE(isOpeningDelimiter(TokenType::COMMA));
    REQUIRE_FALSE(isClosingDelimiter(TokenType::COMMA));
}

// ─────────────────────────────────────────────────────────────────────────────
// COLON vs COLON_COLON
// ─────────────────────────────────────────────────────────────────────────────
//
// The grammar uses ':' as the resource-field separator and '::' as the
// module qualifier. They are distinct token types, not one token with a
// length. The lexer emits COLON_COLON for two consecutive ':' and COLON
// otherwise. These tests pin that the two are never conflated by the
// classification predicates.

TEST_CASE("COLON and COLON_COLON are distinct tokens",
          "[core][tokens][colon]")
{
    REQUIRE(TokenType::COLON != TokenType::COLON_COLON);
    REQUIRE(static_cast<uint16_t>(TokenType::COLON) !=
            static_cast<uint16_t>(TokenType::COLON_COLON));
}

TEST_CASE("COLON and COLON_COLON are both punctuation",
          "[core][tokens][colon]")
{
    REQUIRE(isPunctuation(TokenType::COLON));
    REQUIRE(isPunctuation(TokenType::COLON_COLON));
}

TEST_CASE("COLON and COLON_COLON are neither opening nor closing delimiters",
          "[core][tokens][colon]")
{
    REQUIRE_FALSE(isOpeningDelimiter(TokenType::COLON));
    REQUIRE_FALSE(isClosingDelimiter(TokenType::COLON));
    REQUIRE_FALSE(isOpeningDelimiter(TokenType::COLON_COLON));
    REQUIRE_FALSE(isClosingDelimiter(TokenType::COLON_COLON));
}

TEST_CASE("COLON and COLON_COLON are not keywords or literals",
          "[core][tokens][colon]")
{
    REQUIRE_FALSE(isKeyword(TokenType::COLON));
    REQUIRE_FALSE(isKeyword(TokenType::COLON_COLON));
    REQUIRE_FALSE(isLiteral(TokenType::COLON));
    REQUIRE_FALSE(isLiteral(TokenType::COLON_COLON));
}

// ─────────────────────────────────────────────────────────────────────────────
// literalKindOf
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("literalKindOf: maps each literal token to its kind",
          "[core][tokens][literal]")
{
    REQUIRE(literalKindOf(TokenType::INT_LITERAL) == LiteralKind::Int);
    REQUIRE(literalKindOf(TokenType::FLOAT_LITERAL) == LiteralKind::Float);
    REQUIRE(literalKindOf(TokenType::STRING_LITERAL) == LiteralKind::String);
    REQUIRE(literalKindOf(TokenType::CHAR_LITERAL) == LiteralKind::Char);
    REQUIRE(literalKindOf(TokenType::BOOL_LITERAL) == LiteralKind::Bool);
    REQUIRE(literalKindOf(TokenType::NIL_LITERAL) == LiteralKind::Nil);
}

// ─────────────────────────────────────────────────────────────────────────────
// Token
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Token: default is UNKNOWN with no location", "[core][tokens]")
{
    Token t;
    REQUIRE(t.type == TokenType::UNKNOWN);
    REQUIRE_FALSE(t.value.isValid());
    REQUIRE_FALSE(t.location.isKnown());
}

TEST_CASE("Token: is/isNot compare types", "[core][tokens]")
{
    Token t{TokenType::KW_NODE, InternedString{1}, SourceLocation{1, 1}};
    REQUIRE(t.is(TokenType::KW_NODE));
    REQUIRE(t.isNot(TokenType::KW_RESOURCE));
    REQUIRE_FALSE(t.isEof());
    REQUIRE_FALSE(t.isUnknown());
}

TEST_CASE("Token: EOF and UNKNOWN helpers", "[core][tokens]")
{
    Token eof{TokenType::EOF_TOKEN, InternedString{}, SourceLocation{1, 1}};
    REQUIRE(eof.isEof());
    REQUIRE_FALSE(eof.isUnknown());

    Token unk{TokenType::UNKNOWN, InternedString{5}, SourceLocation{1, 1}};
    REQUIRE(unk.isUnknown());
    REQUIRE_FALSE(unk.isEof());
}

TEST_CASE("Token: hasValue is true when the value is interned",
          "[core][tokens]")
{
    Token a{TokenType::IDENTIFIER, InternedString{1}, SourceLocation{1, 1}};
    REQUIRE(a.hasValue());

    Token b{TokenType::EOF_TOKEN, InternedString{}, SourceLocation{1, 1}};
    REQUIRE_FALSE(b.hasValue());
}

TEST_CASE("Token: a COLON_COLON token carries its spelling",
          "[core][tokens][colon]")
{
    Token t{TokenType::COLON_COLON, InternedString{7}, SourceLocation{1, 1}};
    REQUIRE(t.is(TokenType::COLON_COLON));
    REQUIRE(t.isNot(TokenType::COLON));
    REQUIRE(t.hasValue());
}

// ─────────────────────────────────────────────────────────────────────────────
// Name functions
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("tokenTypeName: returns a spelling for every token",
          "[core][tokens][names]")
{
    REQUIRE(std::string_view(tokenTypeName(TokenType::KW_IMPORT)) == "import");
    REQUIRE(std::string_view(tokenTypeName(TokenType::LPAREN)) == "(");
    REQUIRE(std::string_view(tokenTypeName(TokenType::COLON)) == ":");
    REQUIRE(std::string_view(tokenTypeName(TokenType::COLON_COLON)) == "::");
    REQUIRE(std::string_view(tokenTypeName(TokenType::AT_SIGN)) == "@");
    REQUIRE(std::string_view(tokenTypeName(TokenType::EOF_TOKEN)) == "EOF");
}

TEST_CASE("tokenTypeDescription: returns a description for diagnostics",
          "[core][tokens][names]")
{
    REQUIRE(std::string_view(tokenTypeDescription(TokenType::KW_NODE)) ==
            "'node'");
    REQUIRE(std::string_view(tokenTypeDescription(TokenType::IDENTIFIER)) ==
            "an identifier");
    REQUIRE(std::string_view(tokenTypeDescription(TokenType::COLON)) == "':'");
    REQUIRE(std::string_view(tokenTypeDescription(TokenType::COLON_COLON)) ==
            "'::'");
    REQUIRE(std::string_view(tokenTypeDescription(TokenType::EOF_TOKEN)) ==
            "end of input");
}