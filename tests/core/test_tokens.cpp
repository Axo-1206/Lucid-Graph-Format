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
    REQUIRE(static_cast<uint16_t>(TokenType::KW_IMPORT) <= static_cast<uint16_t>(TokenType::KW_OUTPUT));
}

TEST_CASE("TokenType: every literal is contiguous", "[core][tokens]")
{
    REQUIRE(static_cast<uint16_t>(TokenType::INT_LITERAL) <= static_cast<uint16_t>(TokenType::NIL_LITERAL));
}

TEST_CASE("TokenType: every punctuation is contiguous", "[core][tokens]")
{
    REQUIRE(static_cast<uint16_t>(TokenType::LPAREN) <= static_cast<uint16_t>(TokenType::AT_SIGN));
}

// ─────────────────────────────────────────────────────────────────────────────
// isKeyword
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("isKeyword: true for the nine keywords", "[core][tokens]")
{
    REQUIRE(isKeyword(TokenType::KW_IMPORT));
    REQUIRE(isKeyword(TokenType::KW_FROM));
    REQUIRE(isKeyword(TokenType::KW_ENUM));
    REQUIRE(isKeyword(TokenType::KW_RESOURCE));
    REQUIRE(isKeyword(TokenType::KW_NODE));
    REQUIRE(isKeyword(TokenType::KW_COMPOSITE));
    REQUIRE(isKeyword(TokenType::KW_ON));
    REQUIRE(isKeyword(TokenType::KW_INPUT));
    REQUIRE(isKeyword(TokenType::KW_OUTPUT));
}

TEST_CASE("isKeyword: false for non-keywords", "[core][tokens]")
{
    REQUIRE_FALSE(isKeyword(TokenType::IDENTIFIER));
    REQUIRE_FALSE(isKeyword(TokenType::INT_LITERAL));
    REQUIRE_FALSE(isKeyword(TokenType::LPAREN));
    REQUIRE_FALSE(isKeyword(TokenType::EOF_TOKEN));
    REQUIRE_FALSE(isKeyword(TokenType::UNKNOWN));
}

// ─────────────────────────────────────────────────────────────────────────────
// isDeclarationKeyword
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("isDeclarationKeyword: true for the five declaration keywords",
          "[core][tokens]")
{
    REQUIRE(isDeclarationKeyword(TokenType::KW_IMPORT));
    REQUIRE(isDeclarationKeyword(TokenType::KW_ENUM));
    REQUIRE(isDeclarationKeyword(TokenType::KW_RESOURCE));
    REQUIRE(isDeclarationKeyword(TokenType::KW_NODE));
    REQUIRE(isDeclarationKeyword(TokenType::KW_COMPOSITE));
}

TEST_CASE("isDeclarationKeyword: false for other keywords",
          "[core][tokens]")
{
    REQUIRE_FALSE(isDeclarationKeyword(TokenType::KW_FROM));
    REQUIRE_FALSE(isDeclarationKeyword(TokenType::KW_ON));
    REQUIRE_FALSE(isDeclarationKeyword(TokenType::KW_INPUT));
    REQUIRE_FALSE(isDeclarationKeyword(TokenType::KW_OUTPUT));
}

// ─────────────────────────────────────────────────────────────────────────────
// isCompositeBodyKeyword
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("isCompositeBodyKeyword: true for on/input/output",
          "[core][tokens]")
{
    REQUIRE(isCompositeBodyKeyword(TokenType::KW_ON));
    REQUIRE(isCompositeBodyKeyword(TokenType::KW_INPUT));
    REQUIRE(isCompositeBodyKeyword(TokenType::KW_OUTPUT));
}

TEST_CASE("isCompositeBodyKeyword: false for declaration keywords",
          "[core][tokens]")
{
    REQUIRE_FALSE(isCompositeBodyKeyword(TokenType::KW_IMPORT));
    REQUIRE_FALSE(isCompositeBodyKeyword(TokenType::KW_NODE));
    REQUIRE_FALSE(isCompositeBodyKeyword(TokenType::KW_COMPOSITE));
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
    REQUIRE_FALSE(isLiteral(TokenType::KW_OUTPUT));
    REQUIRE_FALSE(isLiteral(TokenType::LPAREN));
}

// ─────────────────────────────────────────────────────────────────────────────
// isPunctuation and delimiter helpers
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("isPunctuation: true for the eleven punctuation marks",
          "[core][tokens]")
{
    REQUIRE(isPunctuation(TokenType::LPAREN));
    REQUIRE(isPunctuation(TokenType::RPAREN));
    REQUIRE(isPunctuation(TokenType::LBRACE));
    REQUIRE(isPunctuation(TokenType::RBRACE));
    REQUIRE(isPunctuation(TokenType::LBRACKET));
    REQUIRE(isPunctuation(TokenType::RBRACKET));
    REQUIRE(isPunctuation(TokenType::COMMA));
    REQUIRE(isPunctuation(TokenType::DOT));
    REQUIRE(isPunctuation(TokenType::COLON));
    REQUIRE(isPunctuation(TokenType::EQUALS));
    REQUIRE(isPunctuation(TokenType::AT_SIGN));
}

TEST_CASE("isPunctuation: false for identifier and literals",
          "[core][tokens]")
{
    REQUIRE_FALSE(isPunctuation(TokenType::IDENTIFIER));
    REQUIRE_FALSE(isPunctuation(TokenType::INT_LITERAL));
    REQUIRE_FALSE(isPunctuation(TokenType::KW_NODE));
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

// ─────────────────────────────────────────────────────────────────────────────
// Name functions
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("tokenTypeName: returns a spelling for every token",
          "[core][tokens][names]")
{
    REQUIRE(std::string_view(tokenTypeName(TokenType::KW_IMPORT)) == "import");
    REQUIRE(std::string_view(tokenTypeName(TokenType::KW_COMPOSITE)) == "composite");
    REQUIRE(std::string_view(tokenTypeName(TokenType::LPAREN)) == "(");
    REQUIRE(std::string_view(tokenTypeName(TokenType::AT_SIGN)) == "@");
    REQUIRE(std::string_view(tokenTypeName(TokenType::EOF_TOKEN)) == "EOF");
}

TEST_CASE("tokenTypeDescription: returns a description for diagnostics",
          "[core][tokens][names]")
{
    REQUIRE(std::string_view(tokenTypeDescription(TokenType::KW_NODE)) == "'node'");
    REQUIRE(std::string_view(tokenTypeDescription(TokenType::IDENTIFIER)) == "an identifier");
    REQUIRE(std::string_view(tokenTypeDescription(TokenType::EOF_TOKEN)) == "end of input");
}