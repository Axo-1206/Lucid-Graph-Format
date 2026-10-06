/// @file tests/parser/test_parse_composite.cpp
///
/// @brief Tests for the composite parser and its field parsers.

#include "parser/Parser.hpp"

#include "core/Tokens.hpp"
#include "core/ast/DeclAST.hpp"
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
using lucid::parser::parseCompositeDecl;
using lucid::parser::parseCompositeInput;
using lucid::parser::parseCompositeOutput;
using lucid::parser::ParserContext;
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

        template <typename Fn>
        auto run(std::string_view source, Fn fn)
        {
            TokenStream stream(lex(source));
            ParserContext ctx(pool, arena, diag, stream);
            return fn(stream, ctx);
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// parseCompositeDecl — the shape
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseCompositeDecl parses an empty composite", "[parse-composite]")
{
    Fixture f;
    CompositeDeclAST *node = f.run("composite Empty { }",
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseCompositeDecl(s, c);
                                   });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(f.pool.lookupView(node->name) == std::string_view{"Empty"});
    CHECK(node->inputs.empty());
    CHECK(node->outputs.empty());
    CHECK(node->body.empty());
}

TEST_CASE("parseCompositeDecl parses an input block", "[parse-composite]")
{
    Fixture f;
    const char *source =
        "composite Health {\n"
        "    input { max: int }\n"
        "}\n";
    CompositeDeclAST *node = f.run(source,
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseCompositeDecl(s, c);
                                   });
    REQUIRE(node != nullptr);
    REQUIRE(node->hasInputs());
    REQUIRE(node->inputs.size() == 1);
    CompositeInputAST *in = node->inputs[0];
    REQUIRE(in != nullptr);
    CHECK(f.pool.lookupView(in->name) == std::string_view{"max"});
    REQUIRE(in->type != nullptr);
    CHECK(f.pool.lookupView(in->type->name) == std::string_view{"int"});
}

TEST_CASE("parseCompositeDecl parses an output block", "[parse-composite]")
{
    Fixture f;
    const char *source =
        "composite Health {\n"
        "    output { current: int = State.current }\n"
        "}\n";
    CompositeDeclAST *node = f.run(source,
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseCompositeDecl(s, c);
                                   });
    REQUIRE(node != nullptr);
    REQUIRE(node->hasOutputs());
    REQUIRE(node->outputs.size() == 1);
    CompositeOutputAST *out = node->outputs[0];
    REQUIRE(out != nullptr);
    CHECK(f.pool.lookupView(out->name) == std::string_view{"current"});
    REQUIRE(out->type != nullptr);
    REQUIRE(out->value != nullptr);
    CHECK(out->value->isa<FieldAccessValueAST>());
}

TEST_CASE("parseCompositeDecl parses an Event output", "[parse-composite]")
{
    Fixture f;
    const char *source =
        "composite Health {\n"
        "    output { on_death: Event = dead }\n"
        "}\n";
    CompositeDeclAST *node = f.run(source,
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseCompositeDecl(s, c);
                                   });
    REQUIRE(node != nullptr);
    REQUIRE(node->outputs.size() == 1);
    CompositeOutputAST *out = node->outputs[0];
    REQUIRE(out->type != nullptr);
    CHECK(f.pool.lookupView(out->type->name) == std::string_view{"Event"});
    REQUIRE(out->value != nullptr);
    CHECK(out->value->isa<IdentifierValueAST>());
}

TEST_CASE("parseCompositeDecl parses a body with a resource",
          "[parse-composite]")
{
    Fixture f;
    const char *source =
        "composite Health {\n"
        "    resource State { current: int = 0 }\n"
        "}\n";
    CompositeDeclAST *node = f.run(source,
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseCompositeDecl(s, c);
                                   });
    REQUIRE(node != nullptr);
    REQUIRE(node->body.size() == 1);
    CHECK(node->body[0]->isa<ResourceDeclAST>());
}

TEST_CASE("parseCompositeDecl parses a body with a node", "[parse-composite]")
{
    Fixture f;
    const char *source =
        "composite Health {\n"
        "    node init = SetOnStart(State.max, max)\n"
        "}\n";
    CompositeDeclAST *node = f.run(source,
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseCompositeDecl(s, c);
                                   });
    REQUIRE(node != nullptr);
    REQUIRE(node->body.size() == 1);
    CHECK(node->body[0]->isa<NodeDeclAST>());
}

TEST_CASE("parseCompositeDecl parses a body with a node that has a trigger",
          "[parse-composite]")
{
    Fixture f;
    const char *source =
        "composite Health {\n"
        "    node hurt = When(check) on on_hurt\n"
        "}\n";
    CompositeDeclAST *node = f.run(source,
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseCompositeDecl(s, c);
                                   });
    REQUIRE(node != nullptr);
    REQUIRE(node->body.size() == 1);
    REQUIRE(node->body[0]->isa<NodeDeclAST>());
    auto *nd = node->body[0]->as<NodeDeclAST>();
    REQUIRE(nd->hasTriggers());
    REQUIRE(nd->triggers.size() == 1);
}

TEST_CASE("parseCompositeDecl parses input, output, and body together",
          "[parse-composite]")
{
    Fixture f;
    const char *source =
        "composite Health {\n"
        "    input { max: int }\n"
        "    output { current: int = State.current }\n"
        "    resource State { current: int = 0 }\n"
        "    node init = SetOnStart(State.current, max)\n"
        "}\n";
    CompositeDeclAST *node = f.run(source,
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseCompositeDecl(s, c);
                                   });
    REQUIRE(node != nullptr);
    CHECK(node->inputs.size() == 1);
    CHECK(node->outputs.size() == 1);
    CHECK(node->body.size() == 2);
}

TEST_CASE("parseCompositeDecl reports a nested composite",
          "[parse-composite]")
{
    Fixture f;
    // Nested composites are a syntax error: the body dispatch does not
    // accept `composite`.
    const char *source =
        "composite Outer {\n"
        "    composite Inner { }\n"
        "}\n";
    CompositeDeclAST *node = f.run(source,
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseCompositeDecl(s, c);
                                   });
    REQUIRE(node != nullptr);
    CHECK(f.diag.hasErrors());
    // The outer composite is parsed; the inner one is skipped.
    CHECK(node->body.empty());
}

TEST_CASE("parseCompositeDecl reports a missing name", "[parse-composite]")
{
    Fixture f;
    CompositeDeclAST *node = f.run("composite { }",
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseCompositeDecl(s, c);
                                   });
    CHECK(node == nullptr);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code ==
          DiagCode::Syntax_ExpectedCompositeName);
}

// ─────────────────────────────────────────────────────────────────────────────
// parseCompositeInput
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseCompositeInput parses a simple input", "[parse-composite]")
{
    Fixture f;
    CompositeInputAST *node =
        f.run("max: int",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseCompositeInput(s, c);
              });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(f.pool.lookupView(node->name) == std::string_view{"max"});
    REQUIRE(node->type != nullptr);
    CHECK(f.pool.lookupView(node->type->name) == std::string_view{"int"});
}

TEST_CASE("parseCompositeInput parses a qualified type",
          "[parse-composite]")
{
    Fixture f;
    CompositeInputAST *node =
        f.run("key: core.Key",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseCompositeInput(s, c);
              });
    REQUIRE(node != nullptr);
    REQUIRE(node->type != nullptr);
    CHECK(node->type->isQualified());
}

TEST_CASE("parseCompositeInput reports a missing type", "[parse-composite]")
{
    Fixture f;
    CompositeInputAST *node =
        f.run("max",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseCompositeInput(s, c);
              });
    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    CHECK(node->type == nullptr);
    CHECK(f.diag.hasErrors());
}

// ─────────────────────────────────────────────────────────────────────────────
// parseCompositeOutput
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseCompositeOutput parses a data output", "[parse-composite]")
{
    Fixture f;
    CompositeOutputAST *node =
        f.run("current: int = State.current",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseCompositeOutput(s, c);
              });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(f.pool.lookupView(node->name) == std::string_view{"current"});
    REQUIRE(node->type != nullptr);
    CHECK(f.pool.lookupView(node->type->name) == std::string_view{"int"});
    REQUIRE(node->value != nullptr);
    CHECK(node->value->isa<FieldAccessValueAST>());
}

TEST_CASE("parseCompositeOutput parses an Event output", "[parse-composite]")
{
    Fixture f;
    CompositeOutputAST *node =
        f.run("on_death: Event = dead",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseCompositeOutput(s, c);
              });
    REQUIRE(node != nullptr);
    REQUIRE(node->type != nullptr);
    CHECK(f.pool.lookupView(node->type->name) == std::string_view{"Event"});
    REQUIRE(node->value != nullptr);
    CHECK(node->value->isa<IdentifierValueAST>());
}

TEST_CASE("parseCompositeOutput parses an inline-node output",
          "[parse-composite]")
{
    Fixture f;
    CompositeOutputAST *node =
        f.run("percent: float = DivideNode(State.current, State.max)",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseCompositeOutput(s, c);
              });
    REQUIRE(node != nullptr);
    REQUIRE(node->value != nullptr);
    CHECK(node->value->isa<InlineNodeValueAST>());
}

TEST_CASE("parseCompositeOutput reports a missing '='", "[parse-composite]")
{
    Fixture f;
    CompositeOutputAST *node =
        f.run("current: int",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseCompositeOutput(s, c);
              });
    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    CHECK(node->value == nullptr);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code ==
          DiagCode::Syntax_ExpectedOutputBinding);
}

TEST_CASE("parseCompositeOutput reports a missing type", "[parse-composite]")
{
    Fixture f;
    CompositeOutputAST *node =
        f.run("current",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseCompositeOutput(s, c);
              });
    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    CHECK(node->type == nullptr);
    CHECK(f.diag.hasErrors());
}