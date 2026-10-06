/// @file core/Tokens.cpp
/// @brief Name functions for the token types.

#include "core/Tokens.hpp"

namespace
{

    /// The canonical spelling of a token type. One string literal per case, in
    /// the same order as the enum. The array must stay in sync with TokenType;
    /// tokenTypeName checks the index against the array's length.
    const char *kTokenNames[] = {
        // EOF / error
        "EOF",
        "UNKNOWN",

        // Identifier
        "IDENTIFIER",

        // Declaration keywords
        "import",
        "from",
        "enum",
        "resource",
        "node",
        "composite",

        // Composite-body keywords
        "on",
        "input",
        "output",

        // Literals
        "integer literal",
        "float literal",
        "string literal",
        "character literal",
        "boolean literal",
        "nil literal",

        // Punctuation
        "(",
        ")",
        "{",
        "}",
        "[",
        "]",
        ",",
        ".",
        ":",
        "=",
        "@",
    };

} // namespace

const char *tokenTypeName(TokenType t) noexcept
{
    const auto idx = static_cast<size_t>(t);
    if (idx < sizeof(kTokenNames) / sizeof(kTokenNames[0]))
    {
        return kTokenNames[idx];
    }
    return "<unknown token>";
}

const char *tokenTypeDescription(TokenType t) noexcept
{
    switch (t)
    {
    case TokenType::EOF_TOKEN:
        return "end of input";
    case TokenType::UNKNOWN:
        return "an unrecognized token";
    case TokenType::IDENTIFIER:
        return "an identifier";
    case TokenType::KW_IMPORT:
        return "'import'";
    case TokenType::KW_FROM:
        return "'from'";
    case TokenType::KW_ENUM:
        return "'enum'";
    case TokenType::KW_RESOURCE:
        return "'resource'";
    case TokenType::KW_NODE:
        return "'node'";
    case TokenType::KW_COMPOSITE:
        return "'composite'";
    case TokenType::KW_ON:
        return "'on'";
    case TokenType::KW_INPUT:
        return "'input'";
    case TokenType::KW_OUTPUT:
        return "'output'";
    case TokenType::INT_LITERAL:
        return "an integer literal";
    case TokenType::FLOAT_LITERAL:
        return "a float literal";
    case TokenType::STRING_LITERAL:
        return "a string literal";
    case TokenType::CHAR_LITERAL:
        return "a character literal";
    case TokenType::BOOL_LITERAL:
        return "a boolean literal";
    case TokenType::NIL_LITERAL:
        return "nil";
    case TokenType::LPAREN:
        return "'('";
    case TokenType::RPAREN:
        return "')'";
    case TokenType::LBRACE:
        return "'{'";
    case TokenType::RBRACE:
        return "'}'";
    case TokenType::LBRACKET:
        return "'['";
    case TokenType::RBRACKET:
        return "']'";
    case TokenType::COMMA:
        return "','";
    case TokenType::DOT:
        return "'.'";
    case TokenType::COLON:
        return "':'";
    case TokenType::EQUALS:
        return "'='";
    case TokenType::AT_SIGN:
        return "'@'";
    }
    return "a token";
}