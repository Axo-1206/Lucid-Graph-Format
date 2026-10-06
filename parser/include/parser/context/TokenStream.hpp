/// @file parser/context/TokenStream.hpp
///
/// @brief A forward-only cursor over one file's token vector, with
///        lookahead and position save/restore.
///
/// ─── Per-file ─────────────────────────────────────────────────────────────
/// A TokenStream wraps one file's tokens. The parser's entry point
/// constructs it from the output of the lexer and passes it to every
/// parse function. It outlives a single parse function but not the file.
///
/// ─── The cursor invariant ─────────────────────────────────────────────────
/// The cursor is always a valid index into the token vector, or the
/// vector's size when the stream is exhausted. Since the lexer guarantees
/// the final token is EOF_TOKEN, and no comments survive to this layer,
/// every index points at a visible token or at the EOF sentinel.
///
/// There is no comment-skipping. The lexer already dropped line and
/// block comments. This stream has nothing to skip, which is why
/// getPos()/setPos() take and return a plain index with no normalization.
///
/// ─── The sentinel ─────────────────────────────────────────────────────────
/// When the stream is exhausted, peek() and peekNext() return a reference
/// to a single static Token whose type is EOF_TOKEN and whose value is
/// an invalid InternedString. Callers check isAtEnd() or
/// peekType() == TokenType::EOF_TOKEN; they never compare against the
/// sentinel directly.
///
/// ─── Copy of a token ──────────────────────────────────────────────────────
/// Token is TokenType + InternedString + SourceLocation — 16 bytes, no
/// heap. consume() returns by value; the copy is cheap.

#pragma once

#include "core/Tokens.hpp"
#include "core/SourceLocation.hpp"
#include "core/memory/InternedString.hpp"
#include "core/memory/StringPool.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace lucid::parser
{

    /// @brief A forward-only cursor over one file's tokens, with lookahead.
    class TokenStream
    {
    public:
        // ─── Construction ───────────────────────────────────────────────────

        /// @brief Construct from a file's token vector.
        ///
        /// The vector is moved in. The final token must be an EOF_TOKEN; the
        /// lexer guarantees this. The stream never appends to the vector.
        explicit TokenStream(std::vector<Token> tokens);

        TokenStream(const TokenStream &) = delete;
        TokenStream &operator=(const TokenStream &) = delete;
        TokenStream(TokenStream &&) = default;
        TokenStream &operator=(TokenStream &&) = default;

        // ─── Consumption ────────────────────────────────────────────────────

        /// @brief The current token. Does not advance.
        ///
        /// Returns a reference to the EOF sentinel if the stream is exhausted.
        /// The reference is valid until the next call that consumes a token.
        const Token &peek();

        /// @brief Consume and return the current token.
        ///
        /// Returns the EOF sentinel (by value) if the stream is exhausted.
        /// Advances the cursor one token.
        Token consume();

        /// @brief True if the current token has the given type.
        bool check(TokenType type);

        /// @brief True if the current token has any of the given types.
        template <typename... Types>
        bool checkAny(Types... types)
        {
            const TokenType current = peek().type;
            return ((types == current) || ...);
        }

        /// @brief If the current token has the given type, consume it and
        ///        return true. Otherwise leave the stream unchanged and
        ///        return false.
        bool match(TokenType type);

        /// @brief True if the stream is at end-of-input.
        ///
        /// Equivalent to `peek().isEof()`. Provided for readability at call
        /// sites that loop until the end.
        bool isAtEnd();

        // ─── Location ───────────────────────────────────────────────────────

        /// @brief The location of the current token.
        ///
        /// Returns `SourceLocation{1, 1}` if the stream is exhausted. A
        /// diagnostic against an exhausted stream points at start-of-file,
        /// which is the least misleading default.
        SourceLocation currentLoc();

        /// @brief The location of the most recently consumed token.
        ///
        /// Before any token is consumed, returns `SourceLocation{1, 1}`.
        SourceLocation previousLoc() const { return lastConsumedLoc_; }

        // ─── Lookahead ──────────────────────────────────────────────────────

        /// @brief The type of the current token.
        TokenType peekType() { return peek().type; }

        /// @brief The current token's value handle.
        InternedString peekValue() { return peek().value; }

        /// @brief The current token's value as a view into `pool`.
        ///
        /// The caller supplies the pool; the stream holds no reference to it.
        /// Returns an empty view for the EOF sentinel.
        std::string_view peekValueView(const StringPool &pool)
        {
            return pool.lookupView(peek().value);
        }

        /// @brief The type of the token after the current one.
        TokenType peekNextType();

        /// @brief The token after the current one.
        const Token &peekNext();

        /// @brief The token at `offset` tokens past the current one.
        ///
        /// `peekAt(0)` is the current token, `peekAt(1)` is the same as
        /// `peekNext()`, and so on. An offset past end-of-input returns the
        /// EOF sentinel.
        const Token &peekAt(size_t offset);

        // ─── Position save / restore ────────────────────────────────────────
        //
        // Used by the parser's lookahead helpers, which save the position,
        // scan speculatively, and restore.

        /// @brief The current cursor position.
        ///
        /// The value is opaque to the caller. Passing it back to `setPos`
        /// restores the exact state.
        size_t getPos() const noexcept { return pos_; }

        /// @brief Restore a position previously returned by `getPos`.
        void setPos(size_t pos) noexcept { pos_ = pos; }

        // ─── Underlying storage ─────────────────────────────────────────────

        /// @brief The number of tokens, EOF included.
        size_t tokenCount() const noexcept { return tokens_.size(); }

    private:
        std::vector<Token> tokens_;

        /// The cursor. Always a valid index into `tokens_`, or `tokens_.size()`
        /// when the stream is exhausted.
        size_t pos_ = 0;

        /// The location of the most recently consumed token. Used by
        /// previousLoc() so the caller does not have to reach into the vector.
        SourceLocation lastConsumedLoc_{1, 1};

        /// The sentinel returned when the stream is exhausted. A single
        /// static instance, so `peek()` can return a reference without
        /// allocating a fresh token each time.
        static const Token EOF_TOKEN_SENTINEL;
    };

} // namespace lucid::parser