/**
 * @file core/Tokens.hpp
 *
 * @responsibility The token vocabulary for the Lucid Graph Format: the
 *                 TokenType enum, the LiteralKind enum, the Token value
 *                 type, and the classification predicates the parser uses.
 *
 * ─── Design: the token set is the grammar's fixed vocabulary ──────────────
 * The parser recognizes the keywords, punctuation, and literal forms in
 * docs/grammar/LUCID_GRAMMAR.md §1 and nothing else. Every other name — a
 * module name, a resource name, a node type, a field name — is an
 * IDENTIFIER and is resolved by later passes against the program's
 * declarations.
 *
 * ─── Design: nine keywords ────────────────────────────────────────────────
 * §1.2 and §5 list nine keywords. §2.2's import production writes 'as',
 * but 'as' is not in §1.2's keyword set and not in §5's lexer sketch. The
 * consistent reading is that 'as' is an ordinary identifier and the parser
 * matches it by spelling. The parser does the same for any reserved word
 * the grammar might add later.
 *
 * ─── Design: `emits` is not a keyword ─────────────────────────────────────
 * The grammar's §8.3 notes that the `emits` production was removed. Event
 * outputs are declared in the output block with type `Event`. There is no
 * emits declaration, and no emits keyword.
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
 * spelling, interned once). See Token's documentation for details.
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
///
/// Used by the parser to build the correct LiteralValueAST node without
/// inspecting the token's text. The lexer produces one token type per
/// literal kind (INT_LITERAL, FLOAT_LITERAL, ...), so this enum overlaps
/// with the token type set; it exists as a separate type because the AST's
/// LiteralValueAST stores a LiteralKind, not a TokenType.
enum class LiteralKind : uint8_t
{
    Int,    // 42, 0xFF, 0b1010, 0o17
    Float,  // 3.14, 1.0e9
    String, // "..."
    Char,   // 'c', '\n'
    Bool,   // true, false
    Nil,    // nil
};

// ─────────────────────────────────────────────────────────────────────────────
// TokenType
// ─────────────────────────────────────────────────────────────────────────────
//
// Naming convention:
//
//   KW_*        — a keyword. The lexer recognizes it by spelling.
//   *_LITERAL   — a literal form. The lexer produces the raw lexeme.
//   (none)      — punctuation or delimiter, named by its shape.
//   EOF_TOKEN   — end of input. Always the final token.
//   UNKNOWN     — a lexing error; the parser reports and recovers.
//
// The enum is ordered so tokens of the same category are contiguous. The
// parser uses the `isXxx` predicates rather than raw numeric comparisons,
// but the ordering is what makes a switch over TokenType readable.

enum class TokenType : uint16_t
{

    // ─── End of input ───────────────────────────────────────────────────
    EOF_TOKEN = 0,

    // ─── Error recovery ─────────────────────────────────────────────────
    UNKNOWN, // bad character, malformed literal

    // ─── Identifiers ────────────────────────────────────────────────────
    IDENTIFIER, // any name that is not a keyword

    // ─── Declaration keywords ───────────────────────────────────────────
    //
    // §1.2's set. `from` is declared but unused by any production in the
    // current grammar; it is reserved. `as` is deliberately not here; the
    // parser matches it as an identifier.

    KW_IMPORT,    // import
    KW_FROM,      // from (reserved; not used by the current grammar)
    KW_ENUM,      // enum
    KW_RESOURCE,  // resource
    KW_NODE,      // node
    KW_COMPOSITE, // composite

    // ─── Composite-body keywords ────────────────────────────────────────
    //
    // Appear inside a composite declaration. `on` is also used after a
    // node declaration's argument list, for the trigger list.

    KW_ON,     // on
    KW_INPUT,  // input
    KW_OUTPUT, // output

    // ─── Literal tokens ─────────────────────────────────────────────────
    //
    // The lexer produces the token; the payload is the literal's text
    // (already unescaped for strings and chars). The parser classifies
    // into LiteralKind and builds a LiteralValueAST.

    INT_LITERAL,    // decimal, hex, binary, or octal integer
    FLOAT_LITERAL,  // float
    STRING_LITERAL, // "..."
    CHAR_LITERAL,   // 'c' or '\n'
    BOOL_LITERAL,   // true, false
    NIL_LITERAL,    // nil

    // ─── Punctuation ────────────────────────────────────────────────────
    //
    // The grammar's only punctuation set. There are no operators; a
    // value is a literal, an identifier, a field access, or an inline
    // node, and the expression grammar is flat.

    LPAREN,   // (
    RPAREN,   // )
    LBRACE,   // {
    RBRACE,   // }
    LBRACKET, // [
    RBRACKET, // ]
    COMMA,    // ,
    DOT,      // .
    COLON,    // :
    EQUALS,   // =
    AT_SIGN,  // @
};

// ─────────────────────────────────────────────────────────────────────────────
// Token
// ─────────────────────────────────────────────────────────────────────────────

/// @brief A single lexical token.
///
/// The payload's meaning depends on the type:
///   - IDENTIFIER:        the name, interned.
///   - keyword types:     the keyword's spelling, interned. Uniform with
///                        every other token: `value` is always valid.
///   - literal types:     the literal's content, interned. For a string or
///                        char, escapes are already resolved; for a number,
///                        the raw lexeme is stored and the parser / later
///                        passes interpret it.
///   - punctuation types: the punctuation's spelling, interned.
///   - EOF_TOKEN:         an invalid InternedString (id 0).
///   - UNKNOWN:           whatever fragment the lexer could recover, interned.
///
/// `value` is always a valid handle for non-EOF tokens. A caller that wants
/// the text uses `pool.lookupView(tok.value)`; a caller that only needs the
/// token type ignores it. There is no case where the field is uninitialized
/// or holds a stale string.
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
//
// The parser dispatches on these. The enum's ordering is an implementation
// detail; the predicates are the contract.

inline bool isKeyword(TokenType t) noexcept
{
    return t >= TokenType::KW_IMPORT && t <= TokenType::KW_OUTPUT;
}

inline bool isDeclarationKeyword(TokenType t) noexcept
{
    switch (t)
    {
    case TokenType::KW_IMPORT:
    case TokenType::KW_ENUM:
    case TokenType::KW_RESOURCE:
    case TokenType::KW_NODE:
    case TokenType::KW_COMPOSITE:
        return true;
    default:
        return false;
    }
}

inline bool isCompositeBodyKeyword(TokenType t) noexcept
{
    switch (t)
    {
    case TokenType::KW_ON:
    case TokenType::KW_INPUT:
    case TokenType::KW_OUTPUT:
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

/// @brief The LiteralKind for a literal token type.
///
/// The caller must ensure `t` is a literal token type. The mapping is
/// total over the six literal token types and is the only place the two
/// enumerations are related.
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
        return LiteralKind::Nil; // caller error
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