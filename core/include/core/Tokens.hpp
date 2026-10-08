/**
 * @file core/Tokens.hpp
 *
 * @responsibility The token vocabulary for the Lucid Graph Format: the
 *                 TokenType enum, the LiteralKind enum, the Token value
 *                 type, and the classification predicates the parser uses.
 *
 * ─── Design: nine keywords ────────────────────────────────────────────────
 * §1.2 and §5 list nine keywords. §2.2's import production writes 'as',
 * but 'as' is not in §1.2's keyword set and not in §5's lexer sketch. The
 * consistent reading is that 'as' is an ordinary identifier and the parser
 * matches it by spelling. The parser does the same for any reserved word
 * the grammar might add later.
 *
 * ─── Design: true, false, nil are literals, not keywords ──────────────────
 * The grammar classifies them as literal forms (§1.4: BOOL_LIT, NIL_LIT).
 * The lexer produces a literal token for each, not a keyword token.
 *
 * ─── Design: attributes are juxtaposed, not bracketed ─────────────────────
 * The grammar writes `@export` — `@` followed by an identifier. The lexer
 * emits AT_SIGN and the identifier as separate tokens; the parser reads
 * the pair.
 *
 * ─── Design: two token types carry a payload ──────────────────────────────
 * An IDENTIFIER carries the name; a literal token carries the literal's
 * text (already unescaped for strings and chars). Every other token's
 * `value` field is a valid but unused InternedString (usually the token's
 * spelling, interned once).
 */

#pragma once

#include "core/SourceLocation.hpp"
#include "core/memory/InternedString.hpp"

#include <cstdint>
#include <string_view>

// ─────────────────────────────────────────────────────────────────────────────
// LiteralKind
// ─────────────────────────────────────────────────────────────────────────────

/// @brief The kind of a literal token.
enum class LiteralKind : uint8_t
{
    Int,
    Float,
    String,
    Char,
    Bool,
    Nil,
};

// ─────────────────────────────────────────────────────────────────────────────
// TokenType
// ─────────────────────────────────────────────────────────────────────────────

enum class TokenType : uint16_t
{

    // ─── End of input ───────────────────────────────────────────────────
    EOF_TOKEN = 0,

    // ─── Error recovery ─────────────────────────────────────────────────
    UNKNOWN,

    // ─── Identifiers ────────────────────────────────────────────────────
    IDENTIFIER,

    // ─── Declaration keywords ───────────────────────────────────────────
    //
    // `from` is declared but unused by any production in the current
    // grammar; it is reserved. `as` is deliberately not here; the parser
    // matches it as an identifier.

    KW_IMPORT,   // import
    KW_FROM,     // from (reserved; not used by the current grammar)
    KW_ENUM,     // enum
    KW_RESOURCE, // resource
    KW_NODE,     // node

    // ─── Node-body keywords ─────────────────────────────────────────────

    KW_ON, // on

    // ─── Literal tokens ─────────────────────────────────────────────────

    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    CHAR_LITERAL,
    BOOL_LITERAL,
    NIL_LITERAL,

    // ─── Punctuation ────────────────────────────────────────────────────

    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COMMA,
    DOT,
    COLON,
    EQUALS,
    AT_SIGN,
};

// ─────────────────────────────────────────────────────────────────────────────
// Token
// ─────────────────────────────────────────────────────────────────────────────

struct Token
{
    TokenType type = TokenType::UNKNOWN;
    InternedString value;
    SourceLocation location;

    Token() = default;

    Token(TokenType t, InternedString v, SourceLocation loc)
        : type(t), value(v), location(loc) {}

    bool is(TokenType t) const noexcept { return type == t; }
    bool isNot(TokenType t) const noexcept { return type != t; }
    bool isEof() const noexcept { return type == TokenType::EOF_TOKEN; }
    bool isUnknown() const noexcept { return type == TokenType::UNKNOWN; }
    bool hasValue() const noexcept { return value.isValid(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// Classification predicates
// ─────────────────────────────────────────────────────────────────────────────

inline bool isKeyword(TokenType t) noexcept
{
    return t >= TokenType::KW_IMPORT && t <= TokenType::KW_ON;
}

inline bool isDeclarationKeyword(TokenType t) noexcept
{
    switch (t)
    {
    case TokenType::KW_IMPORT:
    case TokenType::KW_ENUM:
    case TokenType::KW_RESOURCE:
    case TokenType::KW_NODE:
        return true;
    default:
        return false;
    }
}

inline bool isLiteral(TokenType t) noexcept
{
    return t >= TokenType::INT_LITERAL && t <= TokenType::NIL_LITERAL;
}

inline bool isPunctuation(TokenType t) noexcept
{
    return t >= TokenType::LPAREN && t <= TokenType::AT_SIGN;
}

inline bool isOpeningDelimiter(TokenType t) noexcept
{
    return t == TokenType::LPAREN || t == TokenType::LBRACE || t == TokenType::LBRACKET;
}

inline bool isClosingDelimiter(TokenType t) noexcept
{
    return t == TokenType::RPAREN || t == TokenType::RBRACE || t == TokenType::RBRACKET;
}

// ─────────────────────────────────────────────────────────────────────────────
// LiteralKind mapping
// ─────────────────────────────────────────────────────────────────────────────

inline LiteralKind literalKindOf(TokenType t) noexcept
{
    switch (t)
    {
    case TokenType::INT_LITERAL:
        return LiteralKind::Int;
    case TokenType::FLOAT_LITERAL:
        return LiteralKind::Float;
    case TokenType::STRING_LITERAL:
        return LiteralKind::String;
    case TokenType::CHAR_LITERAL:
        return LiteralKind::Char;
    case TokenType::BOOL_LITERAL:
        return LiteralKind::Bool;
    case TokenType::NIL_LITERAL:
        return LiteralKind::Nil;
    default:
        return LiteralKind::Nil;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Names
// ─────────────────────────────────────────────────────────────────────────────

/// The canonical spelling of a token type, for diagnostics and JSON dumps.
/// Returns a string literal. Defined in Tokens.cpp.
const char *tokenTypeName(TokenType t) noexcept;

/// A human-readable description of a token type for "expected X, found Y"
/// messages. Defined in Tokens.cpp.
const char *tokenTypeDescription(TokenType t) noexcept;