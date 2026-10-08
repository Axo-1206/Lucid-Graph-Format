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

TEST_CASE("LayoutBuilder formats a three-segment import", "[layout]")
{
    CHECK(fmt("import a.b.c").text == "import a.b.c\n");
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
          "    A,\n"
          "    B,\n"
          "    C,\n"
          "}\n");
}

TEST_CASE("LayoutBuilder normalises enum to trailing-comma form", "[layout]")
{
    CHECK(fmt("enum Key { A, B, }").text ==
          "enum Key {\n"
          "    A,\n"
          "    B,\n"
          "}\n");
}

TEST_CASE("LayoutBuilder preserves a comment on an enum member",
          "[layout]")
{
    CHECK(fmt("enum Key {\n"
              "    A,  -- the A key\n"
              "    B\n"
              "}\n")
              .text ==
          "enum Key {\n"
          "    A,  -- the A key\n"
          "    B,\n"
          "}\n");
}

TEST_CASE("LayoutBuilder preserves a leading comment on an enum member",
          "[layout]")
{
    CHECK(fmt("enum Key {\n"
              "    -- first\n"
              "    A,\n"
              "    B\n"
              "}\n")
              .text ==
          "enum Key {\n"
          "    -- first\n"
          "    A,\n"
          "    B,\n"
          "}\n");
}

TEST_CASE("LayoutBuilder preserves a trailing comment on an attribute",
          "[layout]")
{
    CHECK(fmt("@export  -- force export\nresource R {}\n").text ==
          "@export  -- force export\n"
          "resource R {\n"
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
    // Resource fields are comma-separated. The formatter emits a
    // trailing comma on every field, including the last, matching the
    // enum-member form.
    CHECK(fmt("resource R { hp: int, speed: float = 1.5 }").text ==
          "resource R {\n"
          "    hp: int,\n"
          "    speed: float = 1.5,\n"
          "}\n");
}

TEST_CASE("LayoutBuilder normalises a resource without commas", "[layout]")
{
    // The grammar requires commas between fields, so this source is a
    // parse error. The test is here to document that the formatter does
    // not accept the old no-comma form.
    const FormatResult r = fmt("resource R { hp: int speed: float }");
    CHECK_FALSE(r.ok);
    CHECK(r.text.empty());
}

TEST_CASE("LayoutBuilder formats a qualified field type", "[layout]")
{
    CHECK(fmt("resource R { key: core::Key }").text ==
          "resource R {\n"
          "    key: core::Key,\n"
          "}\n");
}

TEST_CASE("LayoutBuilder formats a resource field with an enum member default",
          "[layout]")
{
    CHECK(fmt("resource R { key_left: Key = Key.A }").text ==
          "resource R {\n"
          "    key_left: Key = Key.A,\n"
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

TEST_CASE("LayoutBuilder formats a qualified node type", "[layout]")
{
    CHECK(fmt("node h = physics::Body(player)").text ==
          "node h = physics::Body(player)\n");
}

TEST_CASE("LayoutBuilder formats an inline node argument", "[layout]")
{
    CHECK(fmt("node a = Foo(Bar(1))").text == "node a = Foo(Bar(1))\n");
}

TEST_CASE("LayoutBuilder formats a qualified inline node argument",
          "[layout]")
{
    CHECK(fmt("node a = Foo(health::Health(100))").text ==
          "node a = Foo(health::Health(100))\n");
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

TEST_CASE("LayoutBuilder formats an attribute on a node", "[layout]")
{
    // The grammar allows an attribute list on any of the four top-level
    // declarations. The formatter emits the attribute on its own line
    // before the declaration.
    CHECK(fmt("@export node a = Foo()").text ==
          "@export\n"
          "node a = Foo()\n");
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

TEST_CASE("LayoutBuilder preserves a trailing comment on a resource field",
          "[layout]")
{
    // The comma is written before the comment, so the comment stays
    // after the field's syntax.
    CHECK(fmt("resource R {\n"
              "    hp: int,  -- the hit points\n"
              "    speed: float\n"
              "}\n")
              .text ==
          "resource R {\n"
          "    hp: int,  -- the hit points\n"
          "    speed: float,\n"
          "}\n");
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
          "  x: int,\n"
          "}\n");
}