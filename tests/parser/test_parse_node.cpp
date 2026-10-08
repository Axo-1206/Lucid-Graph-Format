/// @file tests/parser/test_parse_node.cpp
///
/// @brief Tests for parseNodeExpr, parseArgList, parseTriggerList.

#include "parser/Parser.hpp"

#include "core/Tokens.hpp"
#include "core/ast/ValueAST.hpp"
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
using lucid::parser::parseArgList;
using lucid::parser::parseNodeExpr;
using lucid::parser::ParserContext;
using lucid::parser::parseTriggerList;
using lucid::parser::TokenStream;

namespace
{

    struct Fixture
    {
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag;

        Fixture() : pool(), arena(), diag(&pool) {}

        std::vector<Token> lex(std::string_view source)
        {
            return lucid::lexer::tokenize(source, pool, diag);
        }

        NodeExprAST *runNodeExpr(std::string_view source)
        {
            TokenStream stream(lex(source));
            ParserContext ctx(pool, arena, diag, stream);
            return parseNodeExpr(stream, ctx);
        }

        // For arg list and trigger list tests, the caller has already
        // consumed the opening token. This helper lexes the source and
        // consumes the first token before calling the function.
        ArenaSpan<BaseAST *> runArgList(std::string_view source)
        {
            TokenStream stream(lex(source));
            ParserContext ctx(pool, arena, diag, stream);
            stream.consume(); // the `(`
            return parseArgList(stream, ctx);
        }

        ArenaSpan<InternedString> runTriggerList(std::string_view source)
        {
            TokenStream stream(lex(source));
            ParserContext ctx(pool, arena, diag, stream);
            stream.consume(); // the `on`
            return parseTriggerList(stream, ctx);
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// parseNodeExpr
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseNodeExpr parses a no-argument node expression",
          "[parse-node]")
{
    Fixture f;
    NodeExprAST *node = f.runNodeExpr("EveryFrame()");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    REQUIRE(node->type != nullptr);
    CHECK(f.pool.lookupView(node->type->name) ==
          std::string_view{"EveryFrame"});
    CHECK(node->args.empty());
}

TEST_CASE("parseNodeExpr parses a one-argument node expression",
          "[parse-node]")
{
    Fixture f;
    NodeExprAST *node = f.runNodeExpr("Float32Node(200.0)");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    REQUIRE(node->type != nullptr);
    CHECK(f.pool.lookupView(node->type->name) ==
          std::string_view{"Float32Node"});
    REQUIRE(node->args.size() == 1);
    REQUIRE(node->args[0] != nullptr);
    REQUIRE(node->args[0]->isa<LiteralValueAST>());
    auto *lit = node->args[0]->as<LiteralValueAST>();
    CHECK(lit->kind == LiteralKind::Float);
}

TEST_CASE("parseNodeExpr parses a multi-argument node expression",
          "[parse-node]")
{
    Fixture f;
    NodeExprAST *node = f.runNodeExpr("Damage(body, 10)");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    REQUIRE(node->args.size() == 2);
    REQUIRE(node->args[0] != nullptr);
    CHECK(node->args[0]->isa<IdentifierValueAST>());
    REQUIRE(node->args[1] != nullptr);
    CHECK(node->args[1]->isa<LiteralValueAST>());
}

TEST_CASE("parseNodeExpr parses a qualified node type",
          "[parse-node]")
{
    Fixture f;
    NodeExprAST *node = f.runNodeExpr("health::Health(100)");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    REQUIRE(node->type != nullptr);
    CHECK(node->type->isQualified());
    CHECK(f.pool.lookupView(node->type->qualifier) ==
          std::string_view{"health"});
    CHECK(f.pool.lookupView(node->type->name) ==
          std::string_view{"Health"});
}

TEST_CASE("parseNodeExpr parses a trailing comma in the argument list",
          "[parse-node]")
{
    Fixture f;
    NodeExprAST *node = f.runNodeExpr("Damage(body, 10,)");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    REQUIRE(node->args.size() == 2);
}

TEST_CASE("parseNodeExpr reports a missing argument list",
          "[parse-node]")
{
    Fixture f;
    NodeExprAST *node = f.runNodeExpr("EveryFrame");
    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    REQUIRE(node->type != nullptr); // the type was parsed
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedNodeArgList);
}

TEST_CASE("parseNodeExpr reports a missing closing paren",
          "[parse-node]")
{
    Fixture f;
    NodeExprAST *node = f.runNodeExpr("EveryFrame(");
    REQUIRE(node != nullptr);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedClosing);
}

TEST_CASE("parseNodeExpr propagates a marked argument",
          "[parse-node]")
{
    Fixture f;
    // The `(` as an argument is not a valid value; parseValue returns a
    // marked UnknownAST, and parseNodeExpr should mark the enclosing node.
    NodeExprAST *node = f.runNodeExpr("Foo(()");
    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
}

// ─────────────────────────────────────────────────────────────────────────────
// parseArgList
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseArgList parses an empty list", "[parse-node]")
{
    Fixture f;
    ArenaSpan<BaseAST *> args = f.runArgList("()");
    CHECK(args.empty());
    CHECK(f.diag.empty());
}

TEST_CASE("parseArgList parses a single argument", "[parse-node]")
{
    Fixture f;
    ArenaSpan<BaseAST *> args = f.runArgList("(42)");
    REQUIRE(args.size() == 1);
    REQUIRE(args[0] != nullptr);
    CHECK(args[0]->isa<LiteralValueAST>());
}

TEST_CASE("parseArgList parses multiple arguments", "[parse-node]")
{
    Fixture f;
    ArenaSpan<BaseAST *> args = f.runArgList("(body, 10)");
    REQUIRE(args.size() == 2);
    CHECK(args[0]->isa<IdentifierValueAST>());
    CHECK(args[1]->isa<LiteralValueAST>());
}

TEST_CASE("parseArgList parses a trailing comma", "[parse-node]")
{
    Fixture f;
    ArenaSpan<BaseAST *> args = f.runArgList("(body, 10,)");
    REQUIRE(args.size() == 2);
}

TEST_CASE("parseArgList reports a missing closing paren",
          "[parse-node]")
{
    Fixture f;
    ArenaSpan<BaseAST *> args = f.runArgList("(42");
    REQUIRE(args.size() == 1);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedClosing);
}

// ─────────────────────────────────────────────────────────────────────────────
// parseTriggerList
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseTriggerList parses a single trigger", "[parse-node]")
{
    Fixture f;
    ArenaSpan<InternedString> triggers = f.runTriggerList("on on_hit");
    REQUIRE(triggers.size() == 1);
    CHECK(f.pool.lookupView(triggers[0]) == std::string_view{"on_hit"});
}

TEST_CASE("parseTriggerList parses multiple triggers", "[parse-node]")
{
    Fixture f;
    ArenaSpan<InternedString> triggers =
        f.runTriggerList("on on_hit, on_other");
    REQUIRE(triggers.size() == 2);
    CHECK(f.pool.lookupView(triggers[0]) == std::string_view{"on_hit"});
    CHECK(f.pool.lookupView(triggers[1]) == std::string_view{"on_other"});
}

TEST_CASE("parseTriggerList parses a trailing comma", "[parse-node]")
{
    Fixture f;
    ArenaSpan<InternedString> triggers =
        f.runTriggerList("on on_hit, on_other,");
    REQUIRE(triggers.size() == 2);
}

TEST_CASE("parseTriggerList reports when no trigger follows 'on'",
          "[parse-node]")
{
    Fixture f;
    ArenaSpan<InternedString> triggers = f.runTriggerList("on");
    CHECK(triggers.empty());
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedTriggerList);
}

TEST_CASE("parseTriggerList reports when 'on' is followed by a non-identifier",
          "[parse-node]")
{
    Fixture f;
    ArenaSpan<InternedString> triggers = f.runTriggerList("on 42");
    CHECK(triggers.empty());
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedTriggerList);
}

TEST_CASE("parseNodeExpr propagates a marked type",
          "[parse-node]")
{
    Fixture f;
    // `(42)` starts with `(` — parseTypeId reports and returns a marked
    // TypeIdAST. The enclosing NodeExprAST should be marked too.
    NodeExprAST *node = f.runNodeExpr("(42)");
    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
}