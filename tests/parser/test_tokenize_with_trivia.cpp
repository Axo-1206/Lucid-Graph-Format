/// @file tests/parser/test_tokenize_with_trivia.cpp
///
/// @brief Tests for the lexer's trivia collection.

#include "parser/lexer/Lexer.hpp"

#include "core/Tokens.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/StringPool.hpp"
#include "core/trivia/Trivia.hpp"
#include "core/trivia/TriviaBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using lucid::diag::DiagnosticEngine;
using lucid::lexer::tokenize;
using lucid::lexer::TokenizeResult;
using lucid::lexer::tokenizeWithTrivia;
using lucid::trivia::TriviaBuffer;
using lucid::trivia::TriviaKind;

namespace
{

    struct Fixture
    {
        StringPool pool;
        DiagnosticEngine diag;

        Fixture() : pool(), diag(&pool) {}

        TokenizeResult run(std::string_view source)
        {
            return tokenizeWithTrivia(source, pool, diag);
        }

        std::string text(InternedString s)
        {
            return std::string(pool.lookupView(s));
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// No comments
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("tokenizeWithTrivia on empty input produces no trivia",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("");
    CHECK(r.trivia.empty());
    REQUIRE(r.tokens.size() == 1);
    CHECK(r.tokens[0].type == TokenType::EOF_TOKEN);
}

TEST_CASE("tokenizeWithTrivia on comment-free input produces no trivia",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("node a = Foo()");
    CHECK(r.trivia.empty());
    CHECK(!r.tokens.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Line comments
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("tokenizeWithTrivia collects a line comment", "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("-- hello\n");
    REQUIRE(r.trivia.size() == 1);
    CHECK(r.trivia[0].kind == TriviaKind::LineComment);
    CHECK(f.text(r.trivia[0].text) == " hello");
    CHECK(r.trivia[0].loc.line() == 1);
    CHECK(r.trivia[0].loc.column() == 1);
}

TEST_CASE("tokenizeWithTrivia collects a line comment with no leading space",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("--hello\n");
    REQUIRE(r.trivia.size() == 1);
    CHECK(f.text(r.trivia[0].text) == "hello");
}

TEST_CASE("tokenizeWithTrivia collects a line comment at end of file",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("-- end");
    REQUIRE(r.trivia.size() == 1);
    CHECK(f.text(r.trivia[0].text) == " end");
}

TEST_CASE("tokenizeWithTrivia collects multiple line comments",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("-- one\n-- two\n-- three\n");
    REQUIRE(r.trivia.size() == 3);
    CHECK(f.text(r.trivia[0].text) == " one");
    CHECK(f.text(r.trivia[1].text) == " two");
    CHECK(f.text(r.trivia[2].text) == " three");
}

TEST_CASE("tokenizeWithTrivia collects a line comment between tokens",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("node -- the name\na");
    REQUIRE(r.trivia.size() == 1);
    CHECK(f.text(r.trivia[0].text) == " the name");
    // Tokens: node, a, EOF.
    REQUIRE(r.tokens.size() == 3);
    CHECK(r.tokens[0].type == TokenType::KW_NODE);
    CHECK(r.tokens[1].type == TokenType::IDENTIFIER);
}

// ─────────────────────────────────────────────────────────────────────────────
// Block comments
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("tokenizeWithTrivia collects a block comment", "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("/- hello -/");
    REQUIRE(r.trivia.size() == 1);
    CHECK(r.trivia[0].kind == TriviaKind::BlockComment);
    CHECK(f.text(r.trivia[0].text) == " hello ");
}

TEST_CASE("tokenizeWithTrivia preserves block comment newlines",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("/- line one\n   line two -/");
    REQUIRE(r.trivia.size() == 1);
    CHECK(f.text(r.trivia[0].text) == " line one\n   line two ");
}

TEST_CASE("tokenizeWithTrivia collects a nested block comment",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("/- outer /- inner -/ outer -/");
    REQUIRE(r.trivia.size() == 1);
    CHECK(f.text(r.trivia[0].text) == " outer /- inner -/ outer ");
}

TEST_CASE("tokenizeWithTrivia collects multiple block comments",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("/- one -/ /- two -/");
    REQUIRE(r.trivia.size() == 2);
    CHECK(f.text(r.trivia[0].text) == " one ");
    CHECK(f.text(r.trivia[1].text) == " two ");
}

// ─────────────────────────────────────────────────────────────────────────────
// Mixed and error cases
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("tokenizeWithTrivia collects both kinds in source order",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("-- line\n/- block -/\n-- line again\n");
    REQUIRE(r.trivia.size() == 3);
    CHECK(r.trivia[0].kind == TriviaKind::LineComment);
    CHECK(r.trivia[1].kind == TriviaKind::BlockComment);
    CHECK(r.trivia[2].kind == TriviaKind::LineComment);
}

TEST_CASE("tokenizeWithTrivia collects an unterminated block comment",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("/- never closed");
    REQUIRE(r.trivia.size() == 1);
    CHECK(r.trivia[0].kind == TriviaKind::BlockComment);
    CHECK(f.text(r.trivia[0].text) == " never closed");
    // An error was reported.
    CHECK(f.diag.hasErrors());
}

TEST_CASE("tokenizeWithTrivia on a comment-only file produces only the EOF token",
          "[lexer][trivia]")
{
    Fixture f;
    TokenizeResult r = f.run("-- just a comment\n");
    REQUIRE(r.tokens.size() == 1);
    CHECK(r.tokens[0].type == TokenType::EOF_TOKEN);
    REQUIRE(r.trivia.size() == 1);
}

TEST_CASE("tokenize with the same input discards comments",
          "[lexer][trivia]")
{
    // The old tokenize entry point must behave exactly as before. It
    // discards comments; the token stream must be identical to
    // tokenizeWithTrivia's.
    Fixture f;
    const std::string_view source = "-- a comment\nnode a = Foo()\n";
    auto plainTokens = tokenize(source, f.pool, f.diag);
    auto withTriviaResult = tokenizeWithTrivia(source, f.pool, f.diag);

    // Same token types, same order.
    REQUIRE(plainTokens.size() == withTriviaResult.tokens.size());
    for (size_t i = 0; i < plainTokens.size(); ++i)
    {
        CHECK(plainTokens[i].type == withTriviaResult.tokens[i].type);
    }

    // Only the trivia version produced trivia.
    CHECK(!withTriviaResult.trivia.empty());
}