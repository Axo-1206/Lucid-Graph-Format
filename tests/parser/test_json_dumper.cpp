/// @file tests/parser/test_json_dumper.cpp
///
/// @brief Tests for the AST → JSON dumper.
///
/// ─── Test shape ───────────────────────────────────────────────────────────
/// Each test parses a small source string and dumps the resulting
/// ModuleAST, then compares the dump against the expected JSON. The
/// comparison is exact string comparison.
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

TEST_CASE("dumpModule dumps an import", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("import keys");
    CHECK(json.find(R"("kind":"ImportDecl")") != std::string::npos);
    CHECK(json.find(R"("name":"keys")") != std::string::npos);
    CHECK(json.find(R"("path":"keys")") != std::string::npos);
}

TEST_CASE("dumpModule dumps an aliased import", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("import core.keys as k");
    CHECK(json.find(R"("path":"core.keys")") != std::string::npos);
    CHECK(json.find(R"("name":"k")") != std::string::npos);
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
    CHECK(json.find(R"("members":["W","A","S","D"])") != std::string::npos);
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

TEST_CASE("dumpModule dumps a qualified resource field type",
          "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("resource R { key: core.Key }");
    CHECK(json.find(R"("qualifier":"core")") != std::string::npos);
    CHECK(json.find(R"("name":"Key")") != std::string::npos);
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

TEST_CASE("dumpModule dumps an inline node argument", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("node a = Foo(Bar(1))");
    // The outer NodeExpr has one arg, an InlineNodeValue containing
    // a NodeExpr for Bar.
    CHECK(json.find(R"("kind":"InlineNodeValue")") != std::string::npos);
    CHECK(json.find(R"("kind":"NodeExpr")") != std::string::npos);
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

// ─────────────────────────────────────────────────────────────────────────────
// Composites
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dumpModule dumps an empty composite", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("composite C { }");
    CHECK(json.find(R"("kind":"CompositeDecl")") != std::string::npos);
    CHECK(json.find(R"("inputs":[])") != std::string::npos);
    CHECK(json.find(R"("outputs":[])") != std::string::npos);
    CHECK(json.find(R"("body":[])") != std::string::npos);
}

TEST_CASE("dumpModule dumps a composite input", "[json-dumper]")
{
    Fixture f;
    const std::string json = f.dump("composite C { input { max: int } }");
    CHECK(json.find(R"("kind":"CompositeInput")") != std::string::npos);
    CHECK(json.find(R"("name":"max")") != std::string::npos);
}

TEST_CASE("dumpModule dumps a composite output", "[json-dumper]")
{
    Fixture f;
    const std::string json =
        f.dump("composite C { output { current: int = State.current } }");
    CHECK(json.find(R"("kind":"CompositeOutput")") != std::string::npos);
    CHECK(json.find(R"("kind":"FieldAccessValue")") != std::string::npos);
    CHECK(json.find(R"("object":"State")") != std::string::npos);
    CHECK(json.find(R"("field":"current")") != std::string::npos);
}

TEST_CASE("dumpModule dumps a composite body", "[json-dumper]")
{
    Fixture f;
    const std::string json =
        f.dump("composite C { resource State { x: int } }");
    CHECK(json.find(R"("kind":"ResourceDecl")") != std::string::npos);
    CHECK(json.find(R"("body":[)") != std::string::npos);
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