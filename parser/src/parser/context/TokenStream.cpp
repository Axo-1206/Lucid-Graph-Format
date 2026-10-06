/// @file parser/context/TokenStream.cpp
/// @brief Implementation of TokenStream.

#include "parser/context/TokenStream.hpp"

#include <utility>

namespace lucid::parser
{

    // ─────────────────────────────────────────────────────────────────────────────
    // Sentinel
    // ─────────────────────────────────────────────────────────────────────────────

    // The sentinel is returned by reference when the stream is exhausted. Its
    // location is the default-constructed SourceLocation (value 0), which
    // SourceLocation reports as "unknown". Its value is an invalid
    // InternedString (id 0), which is what a default-constructed handle holds.
    const Token TokenStream::EOF_TOKEN_SENTINEL =
        Token{TokenType::EOF_TOKEN, InternedString{}, SourceLocation{}};

    // ─────────────────────────────────────────────────────────────────────────────
    // Construction
    // ─────────────────────────────────────────────────────────────────────────────

    TokenStream::TokenStream(std::vector<Token> tokens)
        : tokens_(std::move(tokens)) {}

    // ─────────────────────────────────────────────────────────────────────────────
    // Consumption
    // ─────────────────────────────────────────────────────────────────────────────

    const Token &TokenStream::peek()
    {
        if (pos_ >= tokens_.size())
        {
            return EOF_TOKEN_SENTINEL;
        }
        return tokens_[pos_];
    }

    Token TokenStream::consume()
    {
        if (pos_ >= tokens_.size())
        {
            return EOF_TOKEN_SENTINEL;
        }

        // Token is trivially copyable (TokenType + InternedString +
        // SourceLocation). Returning by value is a 16-byte copy, not a heap
        // operation.
        Token result = tokens_[pos_];
        lastConsumedLoc_ = result.location;
        ++pos_;
        return result;
    }

    bool TokenStream::check(TokenType type)
    {
        return peek().type == type;
    }

    bool TokenStream::match(TokenType type)
    {
        if (check(type))
        {
            consume();
            return true;
        }
        return false;
    }

    bool TokenStream::isAtEnd()
    {
        return pos_ >= tokens_.size() || tokens_[pos_].isEof();
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Location
    // ─────────────────────────────────────────────────────────────────────────────

    SourceLocation TokenStream::currentLoc()
    {
        if (pos_ < tokens_.size())
        {
            return tokens_[pos_].location;
        }
        // Start of file. This gives a sensible location for a diagnostic
        // against an exhausted stream.
        return SourceLocation{1, 1};
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Lookahead
    // ─────────────────────────────────────────────────────────────────────────────

    const Token &TokenStream::peekNext()
    {
        const size_t idx = pos_ + 1;
        if (idx >= tokens_.size())
            return EOF_TOKEN_SENTINEL;
        return tokens_[idx];
    }

    TokenType TokenStream::peekNextType()
    {
        return peekNext().type;
    }

    const Token &TokenStream::peekAt(size_t offset)
    {
        const size_t idx = pos_ + offset;
        if (idx >= tokens_.size())
            return EOF_TOKEN_SENTINEL;
        return tokens_[idx];
    }

} // namespace lucid::parser