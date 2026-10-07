/// @file tests/formatter/test_format_options.cpp
///
/// @brief Tests for FormatOptions.
///
/// ─── What there is to test ────────────────────────────────────────────────
/// FormatOptions is a plain struct. The only things worth testing are
/// the defaults, which are part of its contract with the formatter and
/// with any caller that relies on them.

#include "formatter/FormatOptions.hpp"

#include <catch2/catch_test_macros.hpp>

using lucid::formatter::FormatOptions;

TEST_CASE("FormatOptions has four-space indents by default", "[format-options]")
{
    FormatOptions opts;
    CHECK(opts.indent_width == 4);
}

TEST_CASE("FormatOptions uses spaces by default", "[format-options]")
{
    FormatOptions opts;
    CHECK(opts.use_tabs == false);
}