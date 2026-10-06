/// @file tests/parser/test_token_stream.cpp
///
/// @brief Tests for TokenStream.

#include "parser/context/TokenStream.hpp"

#include "core/Tokens.hpp"
#include "core/SourceLocation.hpp"
#include "core/memory/InternedString.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

using lucid::parser::TokenStream;

namespace
{

    // ─── Helpers ──────────────────────────────────────────────────────────────

    /// Build a token. The value handle is the given ID; callers that care
    /// about the payload intern it into a pool first.
    Token mk(TokenType type, InternedString value, uint32_t line = 1,
             uint32_t col = 1)
    {
        return Token{type, value, SourceLocation{line, col}};
    }

    /// Build a small stream from a list of (type, spelling) pairs. The final
    /// EOF is appended automatically.
    TokenStream makeStream(StringPool &pool,
                           std::initializer_list<std::pair<TokenType,
                                                           std::string_view>>
                               items)
    {
        std::vector<Token> toks;
        for (const auto &item : items)
        {
            toks.push_back(mk(item.first, pool.intern(item.second)));
        }
        toks.push_back(mk(TokenType::EOF_TOKEN, InternedString{}));
        return TokenStream{std::move(toks)};
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Construction and empty stream
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TokenStream: an EOF-only stream is at end",
          "[parser][stream]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {});

    REQUIRE(s.isAtEnd());
    REQUIRE(s.peekType() == TokenType::EOF_TOKEN);
}

TEST_CASE("TokenStream: peek on an exhausted stream returns the sentinel",
          "[parser][stream]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {});

    const Token &t = s.peek();
    REQUIRE(t.isEof());
    REQUIRE_FALSE(t.value.isValid());
}

TEST_CASE("TokenStream: consume on an exhausted stream returns the sentinel",
          "[parser][stream]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {});

    Token t = s.consume();
    REQUIRE(t.isEof());
    REQUIRE_FALSE(t.value.isValid());

    REQUIRE(s.isAtEnd());
    REQUIRE(s.peekType() == TokenType::EOF_TOKEN);
}

// ─────────────────────────────────────────────────────────────────────────────
// Consumption
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TokenStream: peek returns the current token",
          "[parser][stream]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::KW_NODE, "node"},
                                         {TokenType::IDENTIFIER, "x"},
                                     });

    REQUIRE(s.peekType() == TokenType::KW_NODE);
    REQUIRE(s.peekValueView(pool) == "node");
    REQUIRE_FALSE(s.isAtEnd());
}

TEST_CASE("TokenStream: consume advances the cursor",
          "[parser][stream]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::KW_NODE, "node"},
                                         {TokenType::IDENTIFIER, "x"},
                                     });

    Token first = s.consume();
    REQUIRE(first.type == TokenType::KW_NODE);
    REQUIRE(pool.lookupView(first.value) == "node");

    REQUIRE(s.peekType() == TokenType::IDENTIFIER);
    REQUIRE(s.peekValueView(pool) == "x");

    Token second = s.consume();
    REQUIRE(second.type == TokenType::IDENTIFIER);
    REQUIRE(s.isAtEnd());
}

TEST_CASE("TokenStream: check compares the current token's type",
          "[parser][stream]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::KW_NODE, "node"},
                                         {TokenType::IDENTIFIER, "x"},
                                     });

    REQUIRE(s.check(TokenType::KW_NODE));
    REQUIRE_FALSE(s.check(TokenType::IDENTIFIER));
    REQUIRE_FALSE(s.check(TokenType::EOF_TOKEN));
}

TEST_CASE("TokenStream: checkAny matches any of the listed types",
          "[parser][stream]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::IDENTIFIER, "x"},
                                     });

    REQUIRE(s.checkAny(TokenType::KW_NODE, TokenType::IDENTIFIER));
    REQUIRE(s.checkAny(TokenType::IDENTIFIER, TokenType::KW_NODE));
    REQUIRE_FALSE(s.checkAny(TokenType::KW_NODE, TokenType::KW_ENUM));
    REQUIRE_FALSE(s.checkAny());
}

TEST_CASE("TokenStream: match consumes on success and does not on failure",
          "[parser][stream]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::KW_NODE, "node"},
                                         {TokenType::IDENTIFIER, "x"},
                                     });

    REQUIRE_FALSE(s.match(TokenType::KW_ENUM));
    REQUIRE(s.peekType() == TokenType::KW_NODE);

    REQUIRE(s.match(TokenType::KW_NODE));
    REQUIRE(s.peekType() == TokenType::IDENTIFIER);
}

// ─────────────────────────────────────────────────────────────────────────────
// Location
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TokenStream: currentLoc returns the current token's location",
          "[parser][stream][location]")
{
    StringPool pool;

    std::vector<Token> toks;
    toks.push_back(mk(TokenType::KW_NODE, pool.intern("node"), 3, 5));
    toks.push_back(mk(TokenType::IDENTIFIER, pool.intern("x"), 3, 10));
    toks.push_back(mk(TokenType::EOF_TOKEN, InternedString{}));

    TokenStream s{std::move(toks)};

    REQUIRE(s.currentLoc().line() == 3);
    REQUIRE(s.currentLoc().column() == 5);

    s.consume();
    REQUIRE(s.currentLoc().line() == 3);
    REQUIRE(s.currentLoc().column() == 10);
}

TEST_CASE("TokenStream: previousLoc returns the last consumed token's location",
          "[parser][stream][location]")
{
    StringPool pool;

    std::vector<Token> toks;
    toks.push_back(mk(TokenType::KW_NODE, pool.intern("node"), 3, 5));
    toks.push_back(mk(TokenType::IDENTIFIER, pool.intern("x"), 3, 10));
    toks.push_back(mk(TokenType::EOF_TOKEN, InternedString{}));

    TokenStream s{std::move(toks)};

    REQUIRE(s.previousLoc().line() == 1);
    REQUIRE(s.previousLoc().column() == 1);

    s.consume();
    REQUIRE(s.previousLoc().line() == 3);
    REQUIRE(s.previousLoc().column() == 5);

    s.consume();
    REQUIRE(s.previousLoc().line() == 3);
    REQUIRE(s.previousLoc().column() == 10);
}

TEST_CASE("TokenStream: currentLoc on an exhausted stream returns (1,1)",
          "[parser][stream][location]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {});

    REQUIRE(s.currentLoc().line() == 1);
    REQUIRE(s.currentLoc().column() == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Lookahead
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TokenStream: peekNext returns the token after the current one",
          "[parser][stream][lookahead]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::KW_NODE, "node"},
                                         {TokenType::IDENTIFIER, "x"},
                                         {TokenType::EQUALS, "="},
                                     });

    REQUIRE(s.peekNextType() == TokenType::IDENTIFIER);
    REQUIRE(s.peekNext().type == TokenType::IDENTIFIER);
    REQUIRE(s.peekNext().value == pool.intern("x"));

    s.consume();
    REQUIRE(s.peekNextType() == TokenType::EQUALS);
}

TEST_CASE("TokenStream: peekNext past end returns the sentinel",
          "[parser][stream][lookahead]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::KW_NODE, "node"},
                                     });

    REQUIRE(s.peekNextType() == TokenType::EOF_TOKEN);
    REQUIRE(s.peekNext().isEof());

    s.consume();
    REQUIRE(s.peekNextType() == TokenType::EOF_TOKEN);
}

TEST_CASE("TokenStream: peekAt with an offset",
          "[parser][stream][lookahead]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::KW_NODE, "node"},
                                         {TokenType::IDENTIFIER, "x"},
                                         {TokenType::EQUALS, "="},
                                         {TokenType::INT_LITERAL, "1"},
                                     });

    REQUIRE(s.peekAt(0).type == TokenType::KW_NODE);
    REQUIRE(s.peekAt(1).type == TokenType::IDENTIFIER);
    REQUIRE(s.peekAt(2).type == TokenType::EQUALS);
    REQUIRE(s.peekAt(3).type == TokenType::INT_LITERAL);
    REQUIRE(s.peekAt(4).type == TokenType::EOF_TOKEN);
    REQUIRE(s.peekAt(100).type == TokenType::EOF_TOKEN);
}

// ─────────────────────────────────────────────────────────────────────────────
// Position save / restore
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TokenStream: getPos and setPos round-trip",
          "[parser][stream][save]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::KW_NODE, "node"},
                                         {TokenType::IDENTIFIER, "x"},
                                         {TokenType::EQUALS, "="},
                                     });

    const size_t start = s.getPos();

    s.consume();
    s.consume();
    REQUIRE(s.peekType() == TokenType::EQUALS);

    s.setPos(start);
    REQUIRE(s.peekType() == TokenType::KW_NODE);
}

TEST_CASE("TokenStream: a speculative scan can be rolled back",
          "[parser][stream][save]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::IDENTIFIER, "a"},
                                         {TokenType::EQUALS, "="},
                                         {TokenType::INT_LITERAL, "1"},
                                     });

    const size_t save = s.getPos();

    while (!s.isAtEnd())
        s.consume();

    s.setPos(save);

    REQUIRE(s.peekType() == TokenType::IDENTIFIER);
    REQUIRE(s.peekValueView(pool) == "a");
}

// ─────────────────────────────────────────────────────────────────────────────
// tokenCount
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TokenStream: tokenCount includes the EOF",
          "[parser][stream]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::KW_NODE, "node"},
                                         {TokenType::IDENTIFIER, "x"},
                                     });

    REQUIRE(s.tokenCount() == 3);
}

// ─────────────────────────────────────────────────────────────────────────────
// A realistic use: peek, then decide, then consume
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TokenStream: the peek-then-consume pattern",
          "[parser][stream][integration]")
{
    StringPool pool;
    TokenStream s = makeStream(pool, {
                                         {TokenType::KW_NODE, "node"},
                                         {TokenType::IDENTIFIER, "speed"},
                                         {TokenType::EQUALS, "="},
                                         {TokenType::IDENTIFIER, "Float32Node"},
                                         {TokenType::LPAREN, "("},
                                         {TokenType::FLOAT_LITERAL, "200.0"},
                                         {TokenType::RPAREN, ")"},
                                     });

    REQUIRE(s.match(TokenType::KW_NODE));

    REQUIRE(s.check(TokenType::IDENTIFIER));
    const std::string_view nodeName = s.peekValueView(pool);
    REQUIRE(nodeName == "speed");
    s.consume();

    REQUIRE(s.match(TokenType::EQUALS));

    REQUIRE(s.check(TokenType::IDENTIFIER));
    const std::string_view typeName = s.peekValueView(pool);
    REQUIRE(typeName == "Float32Node");
    s.consume();

    REQUIRE(s.match(TokenType::LPAREN));

    REQUIRE(s.check(TokenType::FLOAT_LITERAL));
    const std::string_view arg = s.peekValueView(pool);
    REQUIRE(arg == "200.0");
    s.consume();

    REQUIRE(s.match(TokenType::RPAREN));

    REQUIRE(s.isAtEnd());
}