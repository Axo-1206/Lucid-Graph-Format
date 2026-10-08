/// @file tests/parser/test_parse_value.cpp
///
/// @brief Tests for parseValue and parseLiteral.
///
/// ─── Test shape ───────────────────────────────────────────────────────────
/// Each test lexes a small source string and calls parseValue (or
/// parseLiteral) directly. The function under test is public in
/// Parser.hpp, so the test does not go through parseFile.

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
using lucid::parser::parseLiteral;
using lucid::parser::ParserContext;
using lucid::parser::parseValue;
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

        BaseAST *runValue(std::string_view source)
        {
            TokenStream stream(lex(source));
            ParserContext ctx(pool, arena, diag, stream);
            return parseValue(stream, ctx);
        }

        LiteralValueAST *runLiteral(std::string_view source)
        {
            TokenStream stream(lex(source));
            ParserContext ctx(pool, arena, diag, stream);
            return parseLiteral(stream, ctx);
        }

        // Run parseValue on a source and return the resulting stream so
        // the test can inspect what was consumed.
        template <typename Fn>
        auto runWithStream(std::string_view source, Fn fn)
            -> decltype(fn(std::declval<TokenStream &>(),
                           std::declval<ParserContext &>()))
        {
            TokenStream stream(lex(source));
            ParserContext ctx(pool, arena, diag, stream);
            return fn(stream, ctx);
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// parseLiteral — all six literal kinds
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseLiteral parses an integer literal", "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("42");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::Int);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"42"});
}

TEST_CASE("parseLiteral parses a hex integer literal", "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("0xFF");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::Int);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"0xFF"});
}

TEST_CASE("parseLiteral parses a negative integer literal", "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("-7");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::Int);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"-7"});
}

TEST_CASE("parseLiteral parses a float literal", "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("3.14");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::Float);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"3.14"});
}

TEST_CASE("parseLiteral parses a float literal with an exponent",
          "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("1.5e9");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::Float);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"1.5e9"});
}

TEST_CASE("parseLiteral parses a string literal", "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("\"hello\"");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::String);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"hello"});
}

TEST_CASE("parseLiteral parses a string literal with an escape",
          "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("\"line1\\nline2\"");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::String);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"line1\nline2"});
}

TEST_CASE("parseLiteral parses a char literal", "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("'x'");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::Char);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"x"});
}

TEST_CASE("parseLiteral parses a char literal with an escape",
          "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("'\\n'");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::Char);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"\n"});
}

TEST_CASE("parseLiteral parses a bool literal", "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("true");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::Bool);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"true"});
}

TEST_CASE("parseLiteral parses a nil literal", "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("nil");
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(node->kind == LiteralKind::Nil);
    CHECK(f.pool.lookupView(node->text) == std::string_view{"nil"});
}

TEST_CASE("parseLiteral reports when the current token is not a literal",
          "[parse-value]")
{
    Fixture f;
    LiteralValueAST *node = f.runLiteral("Key");
    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedLiteral);
}

// ─────────────────────────────────────────────────────────────────────────────
// parseValue — the four value forms
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseValue parses a literal", "[parse-value]")
{
    Fixture f;
    BaseAST *node = f.runValue("42");
    REQUIRE(node != nullptr);
    REQUIRE(node->isa<LiteralValueAST>());
    CHECK_FALSE(node->hasSyntaxError);
    auto *lit = node->as<LiteralValueAST>();
    CHECK(lit->kind == LiteralKind::Int);
}

TEST_CASE("parseValue parses a bare identifier", "[parse-value]")
{
    Fixture f;
    BaseAST *node = f.runValue("player");
    REQUIRE(node != nullptr);
    REQUIRE(node->isa<IdentifierValueAST>());
    CHECK_FALSE(node->hasSyntaxError);
    auto *id = node->as<IdentifierValueAST>();
    CHECK(f.pool.lookupView(id->name) == std::string_view{"player"});
}

TEST_CASE("parseValue parses a field access", "[parse-value]")
{
    Fixture f;
    BaseAST *node = f.runValue("Config.speed");
    REQUIRE(node != nullptr);
    REQUIRE(node->isa<FieldAccessValueAST>());
    CHECK_FALSE(node->hasSyntaxError);
    auto *fa = node->as<FieldAccessValueAST>();
    CHECK(f.pool.lookupView(fa->object) == std::string_view{"Config"});
    CHECK(f.pool.lookupView(fa->field) == std::string_view{"speed"});
}

TEST_CASE("parseValue parses an enum field access", "[parse-value]")
{
    Fixture f;
    BaseAST *node = f.runValue("Key.W");
    REQUIRE(node != nullptr);
    REQUIRE(node->isa<FieldAccessValueAST>());
    auto *fa = node->as<FieldAccessValueAST>();
    CHECK(f.pool.lookupView(fa->object) == std::string_view{"Key"});
    CHECK(f.pool.lookupView(fa->field) == std::string_view{"W"});
}

TEST_CASE("parseValue parses an inline node with no arguments",
          "[parse-value]")
{
    Fixture f;
    BaseAST *node = f.runValue("EveryFrame()");
    REQUIRE(node != nullptr);
    REQUIRE(node->isa<InlineNodeValueAST>());
    CHECK_FALSE(node->hasSyntaxError);
    auto *inl = node->as<InlineNodeValueAST>();
    REQUIRE(inl->node != nullptr);
    REQUIRE(inl->node->type != nullptr);
    CHECK(f.pool.lookupView(inl->node->type->name) ==
          std::string_view{"EveryFrame"});
    CHECK(inl->node->args.empty());
}

TEST_CASE("parseValue parses an inline node with arguments",
          "[parse-value]")
{
    Fixture f;
    BaseAST *node = f.runValue("Float32Node(200.0)");
    REQUIRE(node != nullptr);
    REQUIRE(node->isa<InlineNodeValueAST>());
    auto *inl = node->as<InlineNodeValueAST>();
    REQUIRE(inl->node != nullptr);
    REQUIRE(inl->node->type != nullptr);
    CHECK(f.pool.lookupView(inl->node->type->name) ==
          std::string_view{"Float32Node"});
    REQUIRE(inl->node->args.size() == 1);
    auto *arg = inl->node->args[0];
    REQUIRE(arg != nullptr);
    REQUIRE(arg->isa<LiteralValueAST>());
}

TEST_CASE("parseValue parses an inline node with a qualified type",
          "[parse-value]")
{
    Fixture f;
    BaseAST *node = f.runValue("health::Health(100)");
    REQUIRE(node != nullptr);
    REQUIRE(node->isa<InlineNodeValueAST>());
    auto *inl = node->as<InlineNodeValueAST>();
    REQUIRE(inl->node != nullptr);
    REQUIRE(inl->node->type != nullptr);
    CHECK(inl->node->type->isQualified());
    CHECK(f.pool.lookupView(inl->node->type->qualifier) ==
          std::string_view{"health"});
    CHECK(f.pool.lookupView(inl->node->type->name) ==
          std::string_view{"Health"});
}

TEST_CASE("parseValue reports when the current token cannot begin a value",
          "[parse-value]")
{
    Fixture f;
    BaseAST *node = f.runValue("(");
    REQUIRE(node != nullptr);
    CHECK(node->isa<UnknownAST>());
    CHECK(node->hasSyntaxError);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedValue);
}

TEST_CASE("parseValue reports a dot with no field name",
          "[parse-value]")
{
    Fixture f;
    BaseAST *node = f.runValue("Config.");
    REQUIRE(node != nullptr);
    REQUIRE(node->isa<FieldAccessValueAST>());
    CHECK(node->hasSyntaxError);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedFieldAccess);
}

// ─────────────────────────────────────────────────────────────────────────────
// Token consumption
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseValue on a bare identifier consumes exactly one token",
          "[parse-value]")
{
    Fixture f;
    TokenStream stream(f.lex("player"));
    ParserContext ctx(f.pool, f.arena, f.diag, stream);
    BaseAST *node = parseValue(stream, ctx);
    REQUIRE(node != nullptr);
    CHECK(stream.isAtEnd());
}

TEST_CASE("parseValue on a field access consumes exactly three tokens",
          "[parse-value]")
{
    Fixture f;
    TokenStream stream(f.lex("Config.speed"));
    ParserContext ctx(f.pool, f.arena, f.diag, stream);
    BaseAST *node = parseValue(stream, ctx);
    REQUIRE(node != nullptr);
    CHECK(stream.isAtEnd());
}

TEST_CASE("parseValue on an inline node consumes the whole node expression",
          "[parse-value]")
{
    Fixture f;
    TokenStream stream(f.lex("Float32Node(200.0)"));
    ParserContext ctx(f.pool, f.arena, f.diag, stream);
    BaseAST *node = parseValue(stream, ctx);
    REQUIRE(node != nullptr);
    CHECK(stream.isAtEnd());
}

TEST_CASE("parseValue on an unparseable token consumes nothing",
          "[parse-value]")
{
    Fixture f;
    TokenStream stream(f.lex("("));
    ParserContext ctx(f.pool, f.arena, f.diag, stream);
    BaseAST *node = parseValue(stream, ctx);
    REQUIRE(node != nullptr);
    // The `(` is still there.
    CHECK(stream.peekType() == TokenType::LPAREN);
}

TEST_CASE("parseValue distinguishes a qualified inline node from a field access",
          "[parse-value]")
{
    Fixture f;

    // `Config.speed` is a field access: the token after the `.` is an
    // identifier, and the token after *that* is not `(`.
    BaseAST *fa = f.runValue("Config.speed");
    REQUIRE(fa != nullptr);
    CHECK(fa->isa<FieldAccessValueAST>());
    CHECK_FALSE(fa->isa<InlineNodeValueAST>());

    // `health::Health(100)` is a qualified inline node: the token after
    // the `::` is an identifier, and the token after *that* is `(`.
    BaseAST *inl = f.runValue("health::Health(100)");
    REQUIRE(inl != nullptr);
    CHECK(inl->isa<InlineNodeValueAST>());
    CHECK_FALSE(inl->isa<FieldAccessValueAST>());
}