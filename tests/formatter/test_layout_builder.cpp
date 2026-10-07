/// @file tests/formatter/test_layout_builder.cpp
///
/// @brief Tests for LayoutBuilder.
///
/// ─── Test shape ───────────────────────────────────────────────────────────
/// Each test parses a small source, formats it, and compares against the
/// expected canonical text. The comparison is exact.
///
/// The parser is exercised by every test; the formatter is the code under
/// test. The parser's correctness is covered by its own tests.

#include "formatter/Formatter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using lucid::formatter::format;
using lucid::formatter::FormatResult;

namespace
{

    FormatResult fmt(std::string_view source)
    {
        return format(source, "test.lucid");
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Module
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LayoutBuilder formats an empty module", "[layout]")
{
    CHECK(fmt("").text == "");
}

TEST_CASE("LayoutBuilder separates top-level declarations by one blank line",
          "[layout]")
{
    CHECK(fmt("node a = Foo()\nnode b = Bar()\n").text ==
          "node a = Foo()\n"
          "\n"
          "node b = Bar()\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Imports
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LayoutBuilder formats an import", "[layout]")
{
    CHECK(fmt("import keys").text == "import keys\n");
}

TEST_CASE("LayoutBuilder formats a dotted import", "[layout]")
{
    CHECK(fmt("import core.keys").text == "import core.keys\n");
}

TEST_CASE("LayoutBuilder formats an aliased import", "[layout]")
{
    CHECK(fmt("import core.keys as k").text == "import core.keys as k\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LayoutBuilder formats an empty enum", "[layout]")
{
    CHECK(fmt("enum E {}").text == "enum E {\n}\n");
}

TEST_CASE("LayoutBuilder formats an enum with members", "[layout]")
{
    CHECK(fmt("enum Key { A, B, C }").text ==
          "enum Key {\n"
          "    A\n"
          "    B\n"
          "    C\n"
          "}\n");
}

TEST_CASE("LayoutBuilder drops the enum trailing comma", "[layout]")
{
    CHECK(fmt("enum Key { A, B, }").text ==
          "enum Key {\n"
          "    A\n"
          "    B\n"
          "}\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Resources
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LayoutBuilder formats an empty resource", "[layout]")
{
    CHECK(fmt("resource R {}").text == "resource R {\n}\n");
}

TEST_CASE("LayoutBuilder formats a resource with fields", "[layout]")
{
    CHECK(fmt("resource R { hp: int speed: float = 1.5 }").text ==
          "resource R {\n"
          "    hp: int\n"
          "    speed: float = 1.5\n"
          "}\n");
}

TEST_CASE("LayoutBuilder formats a qualified field type", "[layout]")
{
    CHECK(fmt("resource R { key: core.Key }").text ==
          "resource R {\n"
          "    key: core.Key\n"
          "}\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Nodes
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LayoutBuilder formats a node with no args", "[layout]")
{
    CHECK(fmt("node a = Foo()").text == "node a = Foo()\n");
}

TEST_CASE("LayoutBuilder formats a node with args", "[layout]")
{
    CHECK(fmt("node a = Foo(1, x, Config.speed)").text ==
          "node a = Foo(1, x, Config.speed)\n");
}

TEST_CASE("LayoutBuilder formats a node with a trigger", "[layout]")
{
    CHECK(fmt("node a = Foo() on on_hit").text ==
          "node a = Foo() on on_hit\n");
}

TEST_CASE("LayoutBuilder formats a node with multiple triggers", "[layout]")
{
    CHECK(fmt("node a = Foo() on on_hit, on_other").text ==
          "node a = Foo() on on_hit, on_other\n");
}

TEST_CASE("LayoutBuilder formats an inline node argument", "[layout]")
{
    CHECK(fmt("node a = Foo(Bar(1))").text == "node a = Foo(Bar(1))\n");
}

TEST_CASE("LayoutBuilder formats a string argument with re-escaping",
          "[layout]")
{
    CHECK(fmt("node a = Foo(\"a\\nb\")").text == "node a = Foo(\"a\\nb\")\n");
}

TEST_CASE("LayoutBuilder formats a char argument with re-escaping",
          "[layout]")
{
    CHECK(fmt("node a = Foo('\\n')").text == "node a = Foo('\\n')\n");
}

TEST_CASE("LayoutBuilder drops the node arg-list trailing comma",
          "[layout]")
{
    CHECK(fmt("node a = Foo(1, 2,)").text == "node a = Foo(1, 2)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Attributes
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LayoutBuilder formats an attribute on a resource", "[layout]")
{
    CHECK(fmt("@export resource R {}").text ==
          "@export\n"
          "resource R {\n"
          "}\n");
}

TEST_CASE("LayoutBuilder formats multiple attributes", "[layout]")
{
    CHECK(fmt("@export @other resource R {}").text ==
          "@export\n"
          "@other\n"
          "resource R {\n"
          "}\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Composites
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LayoutBuilder formats an empty composite", "[layout]")
{
    CHECK(fmt("composite C {}").text == "composite C {\n}\n");
}

TEST_CASE("LayoutBuilder formats a composite with an input block",
          "[layout]")
{
    CHECK(fmt("composite C { input { max: int } }").text ==
          "composite C {\n"
          "    input {\n"
          "        max: int\n"
          "    }\n"
          "}\n");
}

TEST_CASE("LayoutBuilder formats a composite with an output block",
          "[layout]")
{
    CHECK(fmt("composite C { output { x: int = 0 } }").text ==
          "composite C {\n"
          "    output {\n"
          "        x: int = 0\n"
          "    }\n"
          "}\n");
}

TEST_CASE("LayoutBuilder formats a composite with a body", "[layout]")
{
    CHECK(fmt("composite C { resource R { x: int } }").text ==
          "composite C {\n"
          "    resource R {\n"
          "        x: int\n"
          "    }\n"
          "}\n");
}

TEST_CASE("LayoutBuilder separates composite sections by blank lines",
          "[layout]")
{
    CHECK(fmt("composite C {\n"
              "  input { max: int }\n"
              "  output { x: int = 0 }\n"
              "  resource R { x: int }\n"
              "}\n")
              .text ==
          "composite C {\n"
          "    input {\n"
          "        max: int\n"
          "    }\n"
          "\n"
          "    output {\n"
          "        x: int = 0\n"
          "    }\n"
          "\n"
          "    resource R {\n"
          "        x: int\n"
          "    }\n"
          "}\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Comments
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LayoutBuilder preserves a leading comment", "[layout]")
{
    CHECK(fmt("-- a comment\nnode a = Foo()").text ==
          "-- a comment\n"
          "node a = Foo()\n");
}

TEST_CASE("LayoutBuilder preserves a trailing comment", "[layout]")
{
    CHECK(fmt("node a = Foo() -- trailing\nnode b = Bar()").text ==
          "node a = Foo()  -- trailing\n"
          "\n"
          "node b = Bar()\n");
}

TEST_CASE("LayoutBuilder preserves a comment after the last declaration",
          "[layout]")
{
    CHECK(fmt("node a = Foo()\n-- a trailing comment\n").text ==
          "node a = Foo()\n"
          "-- a trailing comment\n");
}

TEST_CASE("LayoutBuilder preserves a block comment", "[layout]")
{
    CHECK(fmt("/- a block -/\nnode a = Foo()").text ==
          "/- a block -/\n"
          "node a = Foo()\n");
}

TEST_CASE("LayoutBuilder preserves leading comments on each declaration kind",
          "[layout]")
{
    // Import.
    CHECK(fmt("-- a\nimport keys").text == "-- a\nimport keys\n");
    // Enum.
    CHECK(fmt("-- a\nenum E {}").text == "-- a\nenum E {\n}\n");
    // Resource.
    CHECK(fmt("-- a\nresource R {}").text == "-- a\nresource R {\n}\n");
    // Node.
    CHECK(fmt("-- a\nnode a = Foo()").text == "-- a\nnode a = Foo()\n");
    // Composite.
    CHECK(fmt("-- a\ncomposite C {}").text == "-- a\ncomposite C {\n}\n");
}

TEST_CASE("LayoutBuilder preserves a comment before an attribute",
          "[layout]")
{
    CHECK(fmt("-- a\n@export\nresource R {}").text ==
          "-- a\n"
          "@export\n"
          "resource R {\n"
          "}\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Idempotence
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LayoutBuilder output is idempotent", "[layout]")
{
    const std::string source =
        "resource R { hp: int speed: float = 1.5 }\n"
        "\n"
        "@export\n"
        "composite C {\n"
        "  input { max: int }\n"
        "  node init = SetOnStart(State.max, max)\n"
        "}\n"
        "\n"
        "-- comment\n"
        "node player = C(100)\n";

    const std::string first = fmt(source).text;
    const std::string second = fmt(first).text;
    CHECK(first == second);
}

// ─────────────────────────────────────────────────────────────────────────────
// FormatOptions
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LayoutBuilder respects a custom indent_width", "[layout]")
{
    lucid::formatter::FormatOptions opts;
    opts.indent_width = 2;
    CHECK(format("resource R { x: int }", "test.lucid", opts).text ==
          "resource R {\n"
          "  x: int\n"
          "}\n");
}