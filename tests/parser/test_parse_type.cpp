/// @file tests/parser/test_parse_type.cpp
///
/// @brief Tests for parseTypeId.
///
/// ─── Test shape ───────────────────────────────────────────────────────────
/// Each test lexes a small source string, wraps the tokens in a
/// TokenStream, and calls parseTypeId directly. The parseTypeId entry
/// point is public in Parser.hpp, so the test does not need to go through
/// parseFile — this is a unit test of one function, not an integration
/// test of the parser.
///
/// The lexer is exercised, but that is incidental: the tests use the
/// lexer because constructing tokens by hand for every case would be
/// tedious, and the lexer is already tested in isolation by
/// test_lexer.cpp. If a test here fails because the lexer produced
/// unexpected tokens, that is a real failure worth investigating — it
/// means parseTypeId's assumptions about the token stream are wrong.

#include "parser/Parser.hpp"

#include "core/Tokens.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"
#include "parser/lexer/Lexer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <utility>
#include <vector>

using lucid::diag::DiagCode;
using lucid::diag::DiagnosticEngine;
using lucid::parser::ParserContext;
using lucid::parser::parseTypeId;
using lucid::parser::TokenStream;

namespace
{

    // ─── Fixture ──────────────────────────────────────────────────────────
    //
    // Owns the four resources a ParserContext needs and exposes a way to
    // build a context plus a way to run parseTypeId on a source string.
    //
    // The token stream is constructed from the lexer's output for `source`.
    // The EOF token is always present; the lexer appends it.

    struct Fixture
    {
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag;

        Fixture() : pool(), arena(), diag(&pool) {}

        // Lex `source` and return the tokens. The caller wraps them in a
        // TokenStream. This is a helper for the pattern below.
        std::vector<Token> lex(std::string_view source)
        {
            return lucid::lexer::tokenize(source, pool, diag);
        }

        // Run parseTypeId against `source`. Returns the node; the
        // diagnostics are in `diag`.
        TypeIdAST *run(std::string_view source)
        {
            TokenStream stream(lex(source));
            ParserContext ctx(pool, arena, diag, stream);
            return parseTypeId(stream, ctx);
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Happy path — unqualified and qualified
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseTypeId parses an unqualified type name",
          "[parse-type]")
{
    Fixture f;
    TypeIdAST *node = f.run("Key");

    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->isSimple());
    CHECK_FALSE(node->isQualified());
    CHECK(f.pool.lookupView(node->name) == std::string_view{"Key"});
    CHECK_FALSE(node->qualifier.isValid());
    CHECK(f.diag.empty());
}

TEST_CASE("parseTypeId parses a qualified type name",
          "[parse-type]")
{
    Fixture f;
    TypeIdAST *node = f.run("core.Key");

    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->isQualified());
    CHECK(f.pool.lookupView(node->qualifier) == std::string_view{"core"});
    CHECK(f.pool.lookupView(node->name) == std::string_view{"Key"});
    CHECK(f.diag.empty());
}

TEST_CASE("parseTypeId records the location of the first identifier",
          "[parse-type]")
{
    Fixture f;
    TypeIdAST *node = f.run("core.Key");

    REQUIRE(node != nullptr);
    CHECK(node->loc.isKnown());
    CHECK(node->loc.line() == 1);
    CHECK(node->loc.column() == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Failure modes
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseTypeId reports when there is no identifier",
          "[parse-type]")
{
    // The source is empty (well, the lexer produces EOF). No identifier
    // to parse.
    Fixture f;
    TypeIdAST *node = f.run("");

    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    CHECK_FALSE(node->name.isValid());
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedIdentifier);
}

TEST_CASE("parseTypeId reports when the current token is not an identifier",
          "[parse-type]")
{
    // The source begins with a literal, not an identifier.
    Fixture f;
    TypeIdAST *node = f.run("42");

    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    CHECK_FALSE(node->name.isValid());
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedIdentifier);
}

TEST_CASE("parseTypeId reports a dot with no identifier after it",
          "[parse-type]")
{
    Fixture f;
    TypeIdAST *node = f.run("core.");

    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    // The qualifier is recorded; the name is not.
    CHECK(node->isQualified());
    CHECK(f.pool.lookupView(node->qualifier) == std::string_view{"core"});
    CHECK_FALSE(node->name.isValid());
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedFieldAccess);
}

TEST_CASE("parseTypeId reports a dot followed by a non-identifier",
          "[parse-type]")
{
    Fixture f;
    TypeIdAST *node = f.run("core.42");

    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    CHECK(node->isQualified());
    CHECK_FALSE(node->name.isValid());
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedFieldAccess);
}

TEST_CASE("parseTypeId reports a third segment as an error",
          "[parse-type]")
{
    // The grammar allows at most one `.`. `a.b.c` is a syntax error.
    Fixture f;
    TypeIdAST *node = f.run("a.b.c");

    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    // The first two segments form a valid qualified node.
    CHECK(f.pool.lookupView(node->qualifier) == std::string_view{"a"});
    CHECK(f.pool.lookupView(node->name) == std::string_view{"b"});
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_UnexpectedToken);
}

// ─────────────────────────────────────────────────────────────────────────────
// Token-stream behavior — what parseTypeId consumes
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseTypeId consumes exactly the identifier on an unqualified name",
          "[parse-type]")
{
    // After parseTypeId on `Key`, the next token should be EOF.
    Fixture f;
    TokenStream stream(f.lex("Key"));
    ParserContext ctx(f.pool, f.arena, f.diag, stream);

    TypeIdAST *node = parseTypeId(stream, ctx);
    REQUIRE(node != nullptr);
    CHECK(stream.isAtEnd());
}

TEST_CASE("parseTypeId consumes exactly three tokens on a qualified name",
          "[parse-type]")
{
    // After parseTypeId on `core.Key`, the next token should be EOF.
    Fixture f;
    TokenStream stream(f.lex("core.Key"));
    ParserContext ctx(f.pool, f.arena, f.diag, stream);

    TypeIdAST *node = parseTypeId(stream, ctx);
    REQUIRE(node != nullptr);
    CHECK(stream.isAtEnd());
}

TEST_CASE("parseTypeId consumes the dot but stops before the bad token",
          "[parse-type]")
{
    // On `core.42`, parseTypeId consumes `core` and `.`, but not `42`.
    // The caller's recovery sees `42`.
    Fixture f;
    TokenStream stream(f.lex("core.42"));
    ParserContext ctx(f.pool, f.arena, f.diag, stream);

    TypeIdAST *node = parseTypeId(stream, ctx);
    REQUIRE(node != nullptr);
    CHECK(stream.peekType() == TokenType::INT_LITERAL);
}

TEST_CASE("parseTypeId does not consume the second dot in a third segment",
          "[parse-type]")
{
    // On `a.b.c`, parseTypeId consumes `a`, `.`, `b`, and stops before the
    // second `.`. The caller's recovery sees the second `.`.
    Fixture f;
    TokenStream stream(f.lex("a.b.c"));
    ParserContext ctx(f.pool, f.arena, f.diag, stream);

    TypeIdAST *node = parseTypeId(stream, ctx);
    REQUIRE(node != nullptr);
    CHECK(stream.peekType() == TokenType::DOT);
}