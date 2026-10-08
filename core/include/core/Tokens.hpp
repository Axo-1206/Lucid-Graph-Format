/**
 * @file core/Tokens.hpp
 *
 * @responsibility The token vocabulary for the Lucid Graph Format: the
 *                 TokenType enum, the LiteralKind enum, the Token value
 *                 type, and the classification predicates the parser uses.
 *
 * ─── Design: keywords ─────────────────────────────────────────────────────
 * §1.2 lists the reserved keywords: import, from, enum, resource, node, on.
 * §2.2's import production writes 'as', but 'as' is not in §1.2's keyword
 * set. The consistent reading is that 'as' is an ordinary identifier and
 * the parser matches it by spelling. The parser does the same for any
 * reserved word the grammar might add later.
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
 *
 * ─── Design: ':' and '::' are distinct tokens ─────────────────────────────
 * The grammar uses ':' as the resource-field separator (`name: type`) and
 * '::' as the module qualifier (`module::Name`). They are separate token
 * types, not one token with a length. The lexer emits COLON_COLON when it
 * sees two consecutive ':' and COLON otherwise. This keeps the grammar
 * LL(1) at the resource-field type position; see Grammar.md §3.4.
 *
 * ─── Design: '[' and ']' are reserved ─────────────────────────────────────
 * LBRACKET and RBRACKET are lexed but no production uses them. They are
 * reserved so that a future array or index syntax does not require a lexer
 * change. A '[' or ']' in source is a syntax error at the parser, not the
 * lexer.
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
    //
    // Grouped: opening delimiters, closing delimiters, separators,
    // accessors, sigils. The order is documentation; isPunctuation uses
    // an explicit switch, not a range check, so the grouping is free to
    // change without breaking classification.

    LPAREN,      // (
    RPAREN,      // )
    LBRACE,      // {
    RBRACE,      // }
    LBRACKET,    // [   reserved; no production uses it
    RBRACKET,    // ]   reserved; no production uses it
    COMMA,       // ,
    DOT,         // .   field access; also part of a float literal
    COLON,       // :   resource-field separator
    COLON_COLON, // ::  module qualifier
    EQUALS,      // =
    AT_SIGN,     // @   attribute sigil
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

/// True for every punctuation token, including the reserved brackets and
/// the two-colon qualifier. The switch is explicit so that adding a token
/// to the enum does not silently change classification.
inline bool isPunctuation(TokenType t) noexcept
{
    switch (t)
    {
    case TokenType::LPAREN:
    case TokenType::RPAREN:
    case TokenType::LBRACE:
    case TokenType::RBRACE:
    case TokenType::LBRACKET:
    case TokenType::RBRACKET:
    case TokenType::COMMA:
    case TokenType::DOT:
    case TokenType::COLON:
    case TokenType::COLON_COLON:
    case TokenType::EQUALS:
    case TokenType::AT_SIGN:
        return true;
    default:
        return false;
    }
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