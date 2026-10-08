/// @file tests/parser/test_parse_decl.cpp
///
/// @brief Tests for the declaration parsers in ParseDecl.cpp.
///
/// ─── Test shape ───────────────────────────────────────────────────────────
/// Each test lexes a small source string and calls the specific parser
/// directly. The parsers are public in Parser.hpp, so the tests do not go
/// through parseFile.
///
/// ─── Function doc comments are the spec ───────────────────────────────────
/// Each function's error behavior is documented in Parser.hpp. The tests
/// here check the documented behaviors, both the happy paths and the
/// error paths.

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
using lucid::parser::parseAttribute;
using lucid::parser::parseAttributeList;
using lucid::parser::parseEnumDecl;
using lucid::parser::parseEnumMemberList;
using lucid::parser::parseImportDecl;
using lucid::parser::parseModulePath;
using lucid::parser::parseNodeDecl;
using lucid::parser::ParserContext;
using lucid::parser::parseResourceDecl;
using lucid::parser::parseResourceField;
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
// parseImportDecl
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseImportDecl parses a simple import", "[parse-decl]")
{
    Fixture f;
    ImportDeclAST *node = f.run("import keys",
                                [](TokenStream &s, ParserContext &c)
                                {
                                    return parseImportDecl(s, c);
                                });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(f.pool.lookupView(node->path) == std::string_view{"keys"});
    CHECK(f.pool.lookupView(node->name) == std::string_view{"keys"});
}

TEST_CASE("parseImportDecl parses a dotted import path", "[parse-decl]")
{
    Fixture f;
    ImportDeclAST *node = f.run("import core.keys",
                                [](TokenStream &s, ParserContext &c)
                                {
                                    return parseImportDecl(s, c);
                                });
    REQUIRE(node != nullptr);
    CHECK(f.pool.lookupView(node->path) == std::string_view{"core.keys"});
    // The module name is the final path segment; there is no `as` clause.
    CHECK(f.pool.lookupView(node->name) == std::string_view{"keys"});
}

TEST_CASE("parseImportDecl parses a three-segment path", "[parse-decl]")
{
    Fixture f;
    ImportDeclAST *node = f.run("import a.b.c",
                                [](TokenStream &s, ParserContext &c)
                                {
                                    return parseImportDecl(s, c);
                                });
    REQUIRE(node != nullptr);
    CHECK(f.pool.lookupView(node->path) == std::string_view{"a.b.c"});
    CHECK(f.pool.lookupView(node->name) == std::string_view{"c"});
}

TEST_CASE("parseImportDecl reports a missing path", "[parse-decl]")
{
    Fixture f;
    ImportDeclAST *node = f.run("import",
                                [](TokenStream &s, ParserContext &c)
                                {
                                    return parseImportDecl(s, c);
                                });
    CHECK(node == nullptr);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedModulePath);
}

// ─────────────────────────────────────────────────────────────────────────────
// parseEnumDecl
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseEnumDecl parses a single-member enum", "[parse-decl]")
{
    Fixture f;
    EnumDeclAST *node = f.run("enum Key { A }",
                              [](TokenStream &s, ParserContext &c)
                              {
                                  return parseEnumDecl(s, c);
                              });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(f.pool.lookupView(node->name) == std::string_view{"Key"});
    REQUIRE(node->members.size() == 1);
    CHECK(f.pool.lookupView(node->members[0]->name) == std::string_view{"A"});
}

TEST_CASE("parseEnumDecl parses a multi-member enum", "[parse-decl]")
{
    Fixture f;
    EnumDeclAST *node = f.run("enum Key { W, A, S, D }",
                              [](TokenStream &s, ParserContext &c)
                              {
                                  return parseEnumDecl(s, c);
                              });
    REQUIRE(node != nullptr);
    REQUIRE(node->members.size() == 4);
    CHECK(f.pool.lookupView(node->members[0]->name) == std::string_view{"W"});
    CHECK(f.pool.lookupView(node->members[3]->name) == std::string_view{"D"});
}

TEST_CASE("parseEnumDecl parses a trailing comma", "[parse-decl]")
{
    Fixture f;
    EnumDeclAST *node = f.run("enum Key { W, A, }",
                              [](TokenStream &s, ParserContext &c)
                              {
                                  return parseEnumDecl(s, c);
                              });
    REQUIRE(node != nullptr);
    REQUIRE(node->members.size() == 2);
}

TEST_CASE("parseEnumDecl reports a missing name", "[parse-decl]")
{
    Fixture f;
    EnumDeclAST *node = f.run("enum { A }",
                              [](TokenStream &s, ParserContext &c)
                              {
                                  return parseEnumDecl(s, c);
                              });
    CHECK(node == nullptr);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedEnumName);
}

TEST_CASE("parseEnumDecl records each member's location",
          "[parse-decl]")
{
    Fixture f;
    EnumDeclAST *node = f.run("enum Key {\n    A,\n    B\n}",
                              [](TokenStream &s, ParserContext &c)
                              {
                                  return parseEnumDecl(s, c);
                              });
    REQUIRE(node != nullptr);
    REQUIRE(node->members.size() == 2);
    CHECK(node->members[0]->loc.line() == 2);
    CHECK(node->members[0]->loc.column() == 5);
    CHECK(node->members[1]->loc.line() == 3);
    CHECK(node->members[1]->loc.column() == 5);
}

// ─────────────────────────────────────────────────────────────────────────────
// parseResourceDecl / parseResourceField
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseResourceDecl parses an empty resource", "[parse-decl]")
{
    Fixture f;
    ResourceDeclAST *node = f.run("resource Empty { }",
                                  [](TokenStream &s, ParserContext &c)
                                  {
                                      return parseResourceDecl(s, c);
                                  });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(f.pool.lookupView(node->name) == std::string_view{"Empty"});
    CHECK(node->fields.empty());
}

TEST_CASE("parseResourceDecl parses a resource with fields", "[parse-decl]")
{
    Fixture f;
    const char *source =
        "resource Player {\n"
        "    speed: float = 200.0\n"
        "    hp: int = 100\n"
        "}\n";
    ResourceDeclAST *node = f.run(source,
                                  [](TokenStream &s, ParserContext &c)
                                  {
                                      return parseResourceDecl(s, c);
                                  });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    REQUIRE(node->fields.size() == 2);

    ResourceFieldAST *speed = node->fields[0];
    REQUIRE(speed != nullptr);
    CHECK(f.pool.lookupView(speed->name) == std::string_view{"speed"});
    REQUIRE(speed->type != nullptr);
    CHECK(f.pool.lookupView(speed->type->name) == std::string_view{"float"});
    REQUIRE(speed->hasDefault());
    // defaultValue is a BaseAST*; the node kind is LiteralValue, and the
    // literal's kind is a separate field inside LiteralValueAST.
    REQUIRE(speed->defaultValue->isa<LiteralValueAST>());
    CHECK(speed->defaultValue->as<LiteralValueAST>()->kind ==
          LiteralKind::Float);

    ResourceFieldAST *hp = node->fields[1];
    REQUIRE(hp != nullptr);
    CHECK(f.pool.lookupView(hp->name) == std::string_view{"hp"});
}

TEST_CASE("parseResourceField parses a field with no default", "[parse-decl]")
{
    Fixture f;
    ResourceFieldAST *node = f.run("hp: int",
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseResourceField(s, c);
                                   });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(f.pool.lookupView(node->name) == std::string_view{"hp"});
    REQUIRE(node->type != nullptr);
    CHECK_FALSE(node->hasDefault());
}

TEST_CASE("parseResourceField parses a field with a default", "[parse-decl]")
{
    Fixture f;
    ResourceFieldAST *node = f.run("hp: int = 100",
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseResourceField(s, c);
                                   });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    REQUIRE(node->hasDefault());
    REQUIRE(node->defaultValue->isa<LiteralValueAST>());
    CHECK(node->defaultValue->as<LiteralValueAST>()->kind ==
          LiteralKind::Int);
}

TEST_CASE("parseResourceField parses a qualified field type", "[parse-decl]")
{
    Fixture f;
    ResourceFieldAST *node = f.run("key: core::Key",
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseResourceField(s, c);
                                   });
    REQUIRE(node != nullptr);
    REQUIRE(node->type != nullptr);
    CHECK(node->type->isQualified());
    CHECK(f.pool.lookupView(node->type->qualifier) ==
          std::string_view{"core"});
    CHECK(f.pool.lookupView(node->type->name) == std::string_view{"Key"});
}

TEST_CASE("parseResourceField parses an enum member default",
          "[parse-decl]")
{
    Fixture f;
    ResourceFieldAST *node =
        f.run("key: Key = Key.A",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseResourceField(s, c);
              });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    REQUIRE(node->hasDefault());
    REQUIRE(node->defaultValue != nullptr);
    CHECK(node->defaultValue->isa<FieldAccessValueAST>());
}

TEST_CASE("parseResourceField reports a missing type", "[parse-decl]")
{
    Fixture f;
    ResourceFieldAST *node = f.run("hp",
                                   [](TokenStream &s, ParserContext &c)
                                   {
                                       return parseResourceField(s, c);
                                   });
    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    CHECK(node->type == nullptr);
    CHECK(f.diag.hasErrors());
}

// ─────────────────────────────────────────────────────────────────────────────
// parseNodeDecl
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseNodeDecl parses a node with no trigger", "[parse-decl]")
{
    Fixture f;
    NodeDeclAST *node = f.run("node speed = Float32Node(200.0)",
                              [](TokenStream &s, ParserContext &c)
                              {
                                  return parseNodeDecl(s, c);
                              });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(f.pool.lookupView(node->name) == std::string_view{"speed"});
    REQUIRE(node->expr != nullptr);
    REQUIRE(node->expr->type != nullptr);
    CHECK(f.pool.lookupView(node->expr->type->name) ==
          std::string_view{"Float32Node"});
    CHECK(node->triggers.empty());
}

TEST_CASE("parseNodeDecl parses a node with a trigger", "[parse-decl]")
{
    Fixture f;
    NodeDeclAST *node =
        f.run("node play_sfx = PlaySound(\"hit.wav\") on on_hit",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseNodeDecl(s, c);
              });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    REQUIRE(node->hasTriggers());
    REQUIRE(node->triggers.size() == 1);
    CHECK(f.pool.lookupView(node->triggers[0]) ==
          std::string_view{"on_hit"});
}

TEST_CASE("parseNodeDecl parses multiple triggers", "[parse-decl]")
{
    Fixture f;
    NodeDeclAST *node =
        f.run("node damage = Damage(body, 10) on on_hit, on_other",
              [](TokenStream &s, ParserContext &c)
              {
                  return parseNodeDecl(s, c);
              });
    REQUIRE(node != nullptr);
    REQUIRE(node->triggers.size() == 2);
    CHECK(f.pool.lookupView(node->triggers[0]) ==
          std::string_view{"on_hit"});
    CHECK(f.pool.lookupView(node->triggers[1]) ==
          std::string_view{"on_other"});
}

TEST_CASE("parseNodeDecl parses a qualified node type", "[parse-decl]")
{
    Fixture f;
    NodeDeclAST *node = f.run("node h = physics::Body(player)",
                              [](TokenStream &s, ParserContext &c)
                              {
                                  return parseNodeDecl(s, c);
                              });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    REQUIRE(node->expr != nullptr);
    REQUIRE(node->expr->type != nullptr);
    CHECK(node->expr->type->isQualified());
    CHECK(f.pool.lookupView(node->expr->type->qualifier) ==
          std::string_view{"physics"});
    CHECK(f.pool.lookupView(node->expr->type->name) ==
          std::string_view{"Body"});
}

TEST_CASE("parseNodeDecl reports a missing name", "[parse-decl]")
{
    Fixture f;
    NodeDeclAST *node = f.run("node = Float32Node(200.0)",
                              [](TokenStream &s, ParserContext &c)
                              {
                                  return parseNodeDecl(s, c);
                              });
    CHECK(node == nullptr);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedNodeName);
}

TEST_CASE("parseNodeDecl reports a missing '='", "[parse-decl]")
{
    Fixture f;
    NodeDeclAST *node = f.run("node speed Float32Node(200.0)",
                              [](TokenStream &s, ParserContext &c)
                              {
                                  return parseNodeDecl(s, c);
                              });
    CHECK(node == nullptr);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedNodeExpr);
}

// ─────────────────────────────────────────────────────────────────────────────
// parseAttribute / parseAttributeList
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseAttribute parses @export", "[parse-decl]")
{
    Fixture f;
    AttributeAST *node = f.run("@export",
                               [](TokenStream &s, ParserContext &c)
                               {
                                   return parseAttribute(s, c);
                               });
    REQUIRE(node != nullptr);
    CHECK_FALSE(node->hasSyntaxError);
    CHECK(f.pool.lookupView(node->name) == std::string_view{"export"});
}

TEST_CASE("parseAttributeList parses an empty list", "[parse-decl]")
{
    Fixture f;
    auto span = f.run("resource X { }",
                      [](TokenStream &s, ParserContext &c)
                      {
                          return parseAttributeList(s, c);
                      });
    CHECK(span.empty());
    CHECK(f.diag.empty());
}

TEST_CASE("parseAttributeList parses a single attribute", "[parse-decl]")
{
    Fixture f;
    auto span = f.run("@export resource X { }",
                      [](TokenStream &s, ParserContext &c)
                      {
                          return parseAttributeList(s, c);
                      });
    REQUIRE(span.size() == 1);
    CHECK(f.pool.lookupView(span[0]->name) == std::string_view{"export"});
}

TEST_CASE("parseAttributeList parses multiple attributes", "[parse-decl]")
{
    // The parser accepts any @name; the set of recognized attributes is a
    // Sema concern. This test uses a second attribute name to exercise the
    // "more than one attribute" path.
    Fixture f;
    auto span = f.run("@export @custom resource X { }",
                      [](TokenStream &s, ParserContext &c)
                      {
                          return parseAttributeList(s, c);
                      });
    REQUIRE(span.size() == 2);
    CHECK(f.pool.lookupView(span[0]->name) == std::string_view{"export"});
    CHECK(f.pool.lookupView(span[1]->name) == std::string_view{"custom"});
}

TEST_CASE("parseAttribute reports a missing name", "[parse-decl]")
{
    Fixture f;
    AttributeAST *node = f.run("@",
                               [](TokenStream &s, ParserContext &c)
                               {
                                   return parseAttribute(s, c);
                               });
    REQUIRE(node != nullptr);
    CHECK(node->hasSyntaxError);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Syntax_ExpectedAttributeName);
}

// ─────────────────────────────────────────────────────────────────────────────
// parseModulePath
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseModulePath parses a simple path", "[parse-decl]")
{
    Fixture f;
    InternedString path = f.run("keys",
                                [](TokenStream &s, ParserContext &c)
                                {
                                    return parseModulePath(s, c);
                                });
    CHECK(f.pool.lookupView(path) == std::string_view{"keys"});
}

TEST_CASE("parseModulePath parses a dotted path", "[parse-decl]")
{
    Fixture f;
    InternedString path = f.run("a.b.c",
                                [](TokenStream &s, ParserContext &c)
                                {
                                    return parseModulePath(s, c);
                                });
    CHECK(f.pool.lookupView(path) == std::string_view{"a.b.c"});
}

TEST_CASE("parseModulePath reports an empty path", "[parse-decl]")
{
    Fixture f;
    InternedString path = f.run("import",
                                [](TokenStream &s, ParserContext &c)
                                {
                                    return parseModulePath(s, c);
                                });
    CHECK_FALSE(path.isValid());
    CHECK(f.diag.hasErrors());
}