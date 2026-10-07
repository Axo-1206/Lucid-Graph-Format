/// @file tests/formatter/test_formatter_api.cpp
///
/// @brief Tests for the formatter's public API.

#include "formatter/FormatOptions.hpp"
#include "formatter/Formatter.hpp"

#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using lucid::diag::DiagCode;
using lucid::formatter::format;
using lucid::formatter::formatModule;
using lucid::formatter::FormatOptions;
using lucid::formatter::FormatResult;

TEST_CASE("FormatResult has safe defaults", "[formatter-api]")
{
    FormatResult r;
    CHECK(r.ok == false);
    CHECK(r.text.empty());
    CHECK(r.diagnostics.empty());
}

TEST_CASE("format on empty input returns an empty string",
          "[formatter-api]")
{
    const FormatResult r = format("", "test.lucid");
    CHECK(r.ok == true);
    CHECK(r.text.empty());
    CHECK(r.diagnostics.empty());
}

TEST_CASE("format on comment-only input preserves the comment",
          "[formatter-api]")
{
    const FormatResult r = format("-- just a comment\n", "test.lucid");
    CHECK(r.ok == true);
    CHECK(r.text == "-- just a comment\n");
}

TEST_CASE("format on a simple node returns canonical text",
          "[formatter-api]")
{
    const FormatResult r = format("node a=Foo()", "test.lucid");
    CHECK(r.ok == true);
    CHECK(r.text == "node a = Foo()\n");
}

TEST_CASE("format reports parse errors and returns an empty text",
          "[formatter-api]")
{
    const FormatResult r = format("node = Foo()", "test.lucid");

    CHECK(r.ok == false);
    CHECK(r.text.empty());
    REQUIRE_FALSE(r.diagnostics.empty());
    CHECK(r.diagnostics.front().code == DiagCode::Syntax_ExpectedNodeName);
}

TEST_CASE("format propagates multiple parse errors",
          "[formatter-api]")
{
    const FormatResult r = format("node\nenum\n", "test.lucid");

    CHECK(r.ok == false);
    CHECK(r.text.empty());
    CHECK(r.diagnostics.size() >= 2);
}

TEST_CASE("formatModule on a null module returns an empty string",
          "[formatter-api]")
{
    StringPool pool;
    const std::string text = formatModule(nullptr, "", pool);
    CHECK(text.empty());
}

TEST_CASE("format accepts explicit FormatOptions",
          "[formatter-api]")
{
    FormatOptions opts;
    opts.indent_width = 2;

    const FormatResult r = format("resource R { hp: int }", "test.lucid", opts);
    CHECK(r.ok == true);
    CHECK(r.text == "resource R {\n  hp: int\n}\n");
}