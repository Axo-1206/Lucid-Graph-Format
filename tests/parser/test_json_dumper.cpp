/// @file tests/parser/test_json_dumper.cpp
///
/// @brief Tests for the AST → JSON dumper.
///
/// ─── Test shape ───────────────────────────────────────────────────────────
/// Each test parses a small source string and dumps the resulting
/// ModuleAST, then compares the dump against the expected JSON. The
/// comparison is exact string comparison where the whole output is
/// predictable, and a substring search where only a fragment of the
/// output matters.
///
/// The parser is exercised by every test; the dumper is the code under
/// test. The parser's own correctness is covered elsewhere.

#include "parser/dump/JSONDumper.hpp"

#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using lucid::diag::DiagnosticEngine;
using lucid::parser::parseFile;
using lucid::parser::ParserContext;
using lucid::parser::TokenStream;
using lucid::parser::dump::dumpModule;

namespace
{

    struct Fixture
    {
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag;
        TokenStream stream;

        Fixture()
            : pool(), arena(), diag(&pool),
              stream(std::vector<Token>{
                  Token{TokenType::EOF_TOKEN, InternedString{},
                        SourceLocation{1, 1}}}) {}

        std::string dump(std::string_view source)
        {
            ParserContext ctx(pool, arena, diag, stream);
            ModuleAST *module = parseFile("test.lucid", source, ctx);
            return dumpModule(module, pool);
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Empty and trivial
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dumpModule dumps an empty module", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("");
    CHECK(json == R"({"kind":"Module","loc":{"line":1,"col":1},"filePath":"test.lucid","declarations":[]})");
}

// ─────────────────────────────────────────────────────────────────────────────
// Imports
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dumpModule dumps a simple import", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("import keys");
    CHECK(json.find(R"("kind":"ImportDecl")") != std::string::npos);
    CHECK(json.find(R"("name":"keys")") != std::string::npos);
    CHECK(json.find(R"("path":"keys")") != std::string::npos);
}

TEST_CASE("dumpModule dumps a dotted import", "[json-dumper]")
{
    // The module name is the final path segment; there is no `as` clause.
    Fixture f;
    const std::string json = f.dump("import core.keys");
    CHECK(json.find(R"("path":"core.keys")") != std::string::npos);
    CHECK(json.find(R"("name":"keys")") != std::string::npos);
}

TEST_CASE("dumpModule dumps a three-segment import path", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("import a.b.c");
    CHECK(json.find(R"("path":"a.b.c")") != std::string::npos);
    CHECK(json.find(R"("name":"c")") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dumpModule dumps an enum", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("enum Key { W, A, S, D }");
    CHECK(json.find(R"("kind":"EnumDecl")") != std::string::npos);
    CHECK(json.find(R"("name":"Key")") != std::string::npos);
    CHECK(json.find(R"("kind":"EnumMember")") != std::string::npos);
    CHECK(json.find(R"("name":"W")") != std::string::npos);
    CHECK(json.find(R"("name":"D")") != std::string::npos);
}

TEST_CASE("dumpModule dumps an empty enum", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("enum Empty { }");
    CHECK(json.find(R"("members":[])") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Resources
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dumpModule dumps a resource with no fields", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("resource Empty { }");
    CHECK(json.find(R"("kind":"ResourceDecl")") != std::string::npos);
    CHECK(json.find(R"("name":"Empty")") != std::string::npos);
    CHECK(json.find(R"("fields":[])") != std::string::npos);
}

TEST_CASE("dumpModule dumps a resource field with no default",
          "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("resource R { hp: int }");
    CHECK(json.find(R"("kind":"ResourceField")") != std::string::npos);
    CHECK(json.find(R"("name":"hp")") != std::string::npos);
    CHECK(json.find(R"("kind":"TypeId")") != std::string::npos);
    CHECK(json.find(R"("name":"int")") != std::string::npos);
    CHECK(json.find(R"("default":null)") != std::string::npos);
}

TEST_CASE("dumpModule dumps a resource field with a default",
          "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("resource R { hp: int = 100 }");
    CHECK(json.find(R"("kind":"LiteralValue")") != std::string::npos);
    CHECK(json.find(R"("literalKind":"Int")") != std::string::npos);
    CHECK(json.find(R"("text":"100")") != std::string::npos);
}

TEST_CASE("dumpModule dumps a resource field with an enum member default",
          "[json-dumper]")
{
    // `key_left: Key = Key.A` — the default is a value, not a literal.
    Fixture f;
    const std::string json = f.dump("resource R { key_left: Key = Key.A }");
    CHECK(json.find(R"("name":"key_left")") != std::string::npos);
    CHECK(json.find(R"("kind":"FieldAccessValue")") != std::string::npos);
    CHECK(json.find(R"("object":"Key")") != std::string::npos);
    CHECK(json.find(R"("field":"A")") != std::string::npos);
}

TEST_CASE("dumpModule dumps a qualified resource field type",
          "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("resource R { key: core::Key }");
    CHECK(json.find(R"("qualifier":"core")") != std::string::npos);
    CHECK(json.find(R"("name":"Key")") != std::string::npos);
}

TEST_CASE("dumpModule dumps an unqualified type with a null qualifier",
          "[json-dumper]")
{
    // The separator does not appear in the JSON; an unqualified type
    // has a null `qualifier`.
    Fixture f;
    const std::string json = f.dump("resource R { hp: int }");
    CHECK(json.find(R"("qualifier":null)") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Nodes
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dumpModule dumps a node with no arguments", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("node a = EveryFrame()");
    CHECK(json.find(R"("kind":"NodeDecl")") != std::string::npos);
    CHECK(json.find(R"("name":"a")") != std::string::npos);
    CHECK(json.find(R"("kind":"NodeExpr")") != std::string::npos);
    CHECK(json.find(R"("args":[])") != std::string::npos);
    CHECK(json.find(R"("triggers":[])") != std::string::npos);
}

TEST_CASE("dumpModule dumps a node with a literal argument",
          "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("node a = Float32Node(200.0)");
    CHECK(json.find(R"("literalKind":"Float")") != std::string::npos);
    CHECK(json.find(R"("text":"200.0")") != std::string::npos);
}

TEST_CASE("dumpModule dumps a node with a trigger", "[json-dumper]")
{
    Fixture f;
    const std::string json =
        f.dump("node a = PlaySound(\"hit.wav\") on on_hit");
    CHECK(json.find(R"("triggers":["on_hit"])") != std::string::npos);
    CHECK(json.find(R"("literalKind":"String")") != std::string::npos);
    CHECK(json.find(R"("text":"hit.wav")") != std::string::npos);
}

TEST_CASE("dumpModule dumps a node with multiple triggers",
          "[json-dumper]")
{
    Fixture f;
    const std::string json =
        f.dump("node a = Damage(body, 10) on on_hit, on_other");
    CHECK(json.find(R"("triggers":["on_hit","on_other"])") !=
          std::string::npos);
}

TEST_CASE("dumpModule dumps a qualified node type", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("node h = physics::Body(player)");
    CHECK(json.find(R"("qualifier":"physics")") != std::string::npos);
    CHECK(json.find(R"("name":"Body")") != std::string::npos);
}

TEST_CASE("dumpModule dumps an inline node argument", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("node a = Foo(Bar(1))");
    // The outer NodeExpr has one arg, an InlineNodeValue containing
    // a NodeExpr for Bar.
    CHECK(json.find(R"("kind":"InlineNodeValue")") != std::string::npos);
    CHECK(json.find(R"("kind":"NodeExpr")") != std::string::npos);
}

TEST_CASE("dumpModule dumps a qualified inline node argument",
          "[json-dumper]")
{
    // `health::Health(100)` as an argument: the inline node's type has
    // a qualifier.
    Fixture f;
    const std::string json = f.dump("node a = Foo(health::Health(100))");
    CHECK(json.find(R"("kind":"InlineNodeValue")") != std::string::npos);
    CHECK(json.find(R"("qualifier":"health")") != std::string::npos);
    CHECK(json.find(R"("name":"Health")") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Attribute
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dumpModule dumps an attribute", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("@export resource R { }");
    CHECK(json.find(R"("kind":"Attribute")") != std::string::npos);
    CHECK(json.find(R"("name":"export")") != std::string::npos);
}

TEST_CASE("dumpModule dumps an attribute on a node", "[json-dumper]")
{
    // The grammar allows an attribute list on any of the four top-level
    // declarations. Sema rejects `@export` on a node, but the parser and
    // the dumper accept it.
    Fixture f;
    const std::string json = f.dump("@export node a = Foo()");
    CHECK(json.find(R"("kind":"Attribute")") != std::string::npos);
    CHECK(json.find(R"("kind":"NodeDecl")") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Unknown
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dumpModule dumps an UnknownAST", "[json-dumper]")
{
    Fixture f;
    // An inline node argument that is not a valid value.
    const std::string json = f.dump("node a = Foo(()");
    CHECK(json.find(R"("kind":"Unknown")") != std::string::npos);
    CHECK(json.find(R"("hasSyntaxError":true)") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Determinism
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dumpModule is deterministic", "[json-dumper]")
{
    Fixture f;
    const std::string source =
        "resource R { hp: int = 100 }\n"
        "node a = Foo(1, 2)\n";
    const std::string first = f.dump(source);

    Fixture g;
    const std::string second = g.dump(source);

    CHECK(first == second);
}