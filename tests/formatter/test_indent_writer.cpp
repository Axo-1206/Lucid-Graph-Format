/// @file tests/formatter/test_indent_writer.cpp
///
/// @brief Tests for IndentWriter.
///
/// ─── Note on the pending indent ───────────────────────────────────────────
/// The writer's indent is written lazily: on the next `write()` after a
/// `newline()`, not during the `newline()` itself. Tests that check the
/// string immediately after a `newline()` therefore see a `\n` but no
/// trailing indent. Tests that check after a following `write()` see
/// the indent in place. This is deliberate; see the header's doc
/// comment for the reasoning.

#include "formatter/FormatOptions.hpp"
#include "formatter/IndentWriter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using lucid::formatter::FormatOptions;
using lucid::formatter::IndentWriter;

// ─────────────────────────────────────────────────────────────────────────────
// Empty state
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IndentWriter starts empty", "[indent-writer]")
{
    IndentWriter w;
    CHECK(w.str().empty());
    CHECK(w.column() == 0);
    CHECK(w.indentLevel() == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// write
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IndentWriter writes text at column 0", "[indent-writer]")
{
    IndentWriter w;
    w.write("hello");
    CHECK(w.str() == "hello");
    CHECK(w.column() == 5);
}

TEST_CASE("IndentWriter writes consecutive text", "[indent-writer]")
{
    IndentWriter w;
    w.write("hello");
    w.write(", world");
    CHECK(w.str() == "hello, world");
    CHECK(w.column() == 12);
}

TEST_CASE("IndentWriter writes a single char", "[indent-writer]")
{
    IndentWriter w;
    w.write('x');
    w.write('y');
    CHECK(w.str() == "xy");
    CHECK(w.column() == 2);
}

TEST_CASE("IndentWriter writes an empty string without changing the column",
          "[indent-writer]")
{
    IndentWriter w;
    w.write("hello");
    w.write("");
    CHECK(w.str() == "hello");
    CHECK(w.column() == 5);
}

// ─────────────────────────────────────────────────────────────────────────────
// newline
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IndentWriter newline at indent 0 emits just \\n",
          "[indent-writer]")
{
    IndentWriter w;
    w.write("a");
    w.newline();
    CHECK(w.str() == "a\n");
    CHECK(w.column() == 0);
}

TEST_CASE("IndentWriter newline does not write the indent until the next write",
          "[indent-writer]")
{
    IndentWriter w;
    w.indent();
    w.write("a");
    w.newline();
    // Immediately after newline, the indent is not yet in the buffer.
    CHECK(w.str() == "a\n");
    CHECK(w.column() == 0);
    // The next write flushes the indent.
    w.write("b");
    CHECK(w.str() == "a\n    b");
    CHECK(w.column() == 5);
}

TEST_CASE("IndentWriter newline at indent 2 writes eight spaces on the next write",
          "[indent-writer]")
{
    IndentWriter w;
    w.indent();
    w.indent();
    w.write("a");
    w.newline();
    CHECK(w.str() == "a\n");
    w.write("b");
    CHECK(w.str() == "a\n        b");
    CHECK(w.column() == 9);
}

TEST_CASE("IndentWriter emits the indent before any text",
          "[indent-writer]")
{
    IndentWriter w;
    w.indent();
    w.newline();
    w.write("a");
    CHECK(w.str() == "\n    a");
    CHECK(w.column() == 5);
}

// ─────────────────────────────────────────────────────────────────────────────
// Blank lines
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IndentWriter blank line has no trailing whitespace",
          "[indent-writer]")
{
    // Two newlines in a row should produce a truly blank line. The
    // pending indent is not written for the blank line.
    IndentWriter w;
    w.indent();
    w.write("a");
    w.newline();
    w.newline();
    CHECK(w.str() == "a\n\n");
    w.write("b");
    CHECK(w.str() == "a\n\n    b");
}

// ─────────────────────────────────────────────────────────────────────────────
// indent and dedent
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IndentWriter indent raises the level", "[indent-writer]")
{
    IndentWriter w;
    w.indent();
    CHECK(w.indentLevel() == 1);
    w.indent();
    CHECK(w.indentLevel() == 2);
}

TEST_CASE("IndentWriter dedent lowers the level", "[indent-writer]")
{
    IndentWriter w;
    w.indent();
    w.indent();
    w.dedent();
    CHECK(w.indentLevel() == 1);
}

TEST_CASE("IndentWriter dedent clamps at zero", "[indent-writer]")
{
    IndentWriter w;
    w.dedent();
    CHECK(w.indentLevel() == 0);
    w.dedent();
    CHECK(w.indentLevel() == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Custom options
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IndentWriter respects a custom indent_width", "[indent-writer]")
{
    FormatOptions opts;
    opts.indent_width = 2;
    IndentWriter w(opts);
    w.indent();
    w.write("a");
    w.newline();
    w.write("b");
    CHECK(w.str() == "a\n  b");
    CHECK(w.column() == 3);
}

TEST_CASE("IndentWriter respects use_tabs", "[indent-writer]")
{
    FormatOptions opts;
    opts.use_tabs = true;
    IndentWriter w(opts);
    w.indent();
    w.indent();
    w.write("a");
    w.newline();
    w.write("b");
    CHECK(w.str() == "a\n\t\tb");
    CHECK(w.column() == 3);
}

TEST_CASE("IndentWriter use_tabs ignores indent_width", "[indent-writer]")
{
    FormatOptions opts;
    opts.use_tabs = true;
    opts.indent_width = 8; // ignored
    IndentWriter w(opts);
    w.indent();
    w.write("a");
    w.newline();
    w.write("b");
    CHECK(w.str() == "a\n\tb");
    CHECK(w.column() == 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Mixed operations
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IndentWriter writes a small block", "[indent-writer]")
{
    // resource R {
    //     hp: int
    // }
    IndentWriter w;
    w.write("resource R {");
    w.newline();
    w.indent();
    w.write("hp: int");
    w.newline();
    w.dedent();
    w.write("}");

    CHECK(w.str() == "resource R {\n    hp: int\n}");
    CHECK(w.column() == 1);
}

TEST_CASE("IndentWriter column tracks correctly across many writes",
          "[indent-writer]")
{
    IndentWriter w;
    w.write("abc"); // column 3
    w.write("def"); // column 6
    w.newline();    // column 0
    w.write("x");   // column 1 (indent is still 0)
    w.indent();
    w.newline();  // column 0, pending indent of 4
    w.write("y"); // column 5 (4 indent + 1)

    CHECK(w.str() == "abcdef\nx\n    y");
    CHECK(w.column() == 5);
}

// ─────────────────────────────────────────────────────────────────────────────
// takeStr
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IndentWriter takeStr moves the buffer out", "[indent-writer]")
{
    IndentWriter w;
    w.write("hello");
    std::string s = w.takeStr();
    CHECK(s == "hello");
    CHECK(w.str().empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// options accessor
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IndentWriter options returns the construction options",
          "[indent-writer]")
{
    FormatOptions opts;
    opts.indent_width = 2;
    opts.use_tabs = false;
    IndentWriter w(opts);
    CHECK(w.options().indent_width == 2);
    CHECK(w.options().use_tabs == false);
}