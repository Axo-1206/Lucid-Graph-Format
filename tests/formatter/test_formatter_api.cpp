/// @file tests/formatter/test_formatter_api.cpp
///
/// @brief Tests for the formatter's public API.
///
/// ─── What these tests cover ───────────────────────────────────────────────
/// The shape of FormatResult, the `format` entry point, and
/// `formatModule`'s handling of a null module.
///
/// ─── What these tests do not cover ────────────────────────────────────────
/// The actual formatting behavior. In Step 5.3, formatModule is a stub
/// and `format` reports a not-implemented diagnostic for every input
/// that parses cleanly. The tests below assert on that diagnostic.
/// Step 5.4 replaces the stub and updates these tests.

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

// ─────────────────────────────────────────────────────────────────────────────
// FormatResult defaults
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("FormatResult has safe defaults", "[formatter-api]")
{
    FormatResult r;
    CHECK(r.ok == false);
    CHECK(r.text.empty());
    CHECK(r.diagnostics.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// format — parse errors
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("format reports parse errors and returns an empty text",
          "[formatter-api]")
{
    // `node = Foo()` has no name after `node`.
    const FormatResult r = format("node = Foo()", "test.lucid");

    CHECK(r.ok == false);
    CHECK(r.text.empty());
    REQUIRE_FALSE(r.diagnostics.empty());
    // The first diagnostic is the parse error.
    CHECK(r.diagnostics.front().code == DiagCode::Syntax_ExpectedNodeName);
}

TEST_CASE("format propagates multiple parse errors",
          "[formatter-api]")
{
    const FormatResult r = format("node\nenum\n", "test.lucid");

    CHECK(r.ok == false);
    CHECK(r.text.empty());
    // Both declarations are malformed; both produce diagnostics.
    CHECK(r.diagnostics.size() >= 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// format — clean input (Step 5.3 behavior: not-implemented diagnostic)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("format on clean input reports not-implemented in Step 5.3",
          "[formatter-api]")
{
    // The stub formatModule returns an empty string. Until Step 5.4,
    // `format` reports a not-implemented diagnostic so a caller cannot
    // mistake the empty text for a valid result.
    const FormatResult r = format("", "test.lucid");

    CHECK(r.ok == false);
    CHECK(r.text.empty());
    REQUIRE_FALSE(r.diagnostics.empty());
    CHECK(r.diagnostics.back().code == DiagCode::Internal_NotImplemented);
}

TEST_CASE("format on an empty file returns the not-implemented diagnostic",
          "[formatter-api]")
{
    const FormatResult r = format("", "test.lucid");
    CHECK_FALSE(r.ok);
    REQUIRE_FALSE(r.diagnostics.empty());
}

TEST_CASE("format on comment-only input returns the not-implemented diagnostic",
          "[formatter-api]")
{
    const FormatResult r = format("-- just a comment\n", "test.lucid");
    CHECK_FALSE(r.ok);
    REQUIRE_FALSE(r.diagnostics.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// formatModule — null module
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("formatModule on a null module returns an empty string",
          "[formatter-api]")
{
    StringPool pool;
    const std::string text = formatModule(nullptr, "", pool);
    CHECK(text.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// FormatOptions defaults flow through format
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("format accepts explicit FormatOptions", "[formatter-api]")
{
    // The options are passed through to formatModule. In Step 5.3
    // formatModule ignores them; the test verifies that passing them
    // does not break the call.
    FormatOptions opts;
    opts.indent_width = 2;
    opts.use_tabs = true;

    const FormatResult r = format("", "test.lucid", opts);
    // The result is the same as with default options.
    CHECK_FALSE(r.ok);
    REQUIRE_FALSE(r.diagnostics.empty());
    CHECK(r.diagnostics.back().code == DiagCode::Internal_NotImplemented);
}