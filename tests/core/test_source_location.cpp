/// @file tests/core/test_source_location.cpp
///
/// @brief Tests for SourceLocation: packing, accessors, limits, and
///        streaming.

#include "core/SourceLocation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// Construction and accessors
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("SourceLocation: default is unknown", "[core][location]") {
    SourceLocation loc;
    REQUIRE(loc.value == 0);
    REQUIRE_FALSE(loc.isKnown());
    REQUIRE(loc.line() == 0);
    REQUIRE(loc.column() == 0);
}

TEST_CASE("SourceLocation: line and column round-trip", "[core][location]") {
    SourceLocation loc{1, 1};
    REQUIRE(loc.isKnown());
    REQUIRE(loc.line() == 1);
    REQUIRE(loc.column() == 1);

    SourceLocation loc2{42, 7};
    REQUIRE(loc2.line() == 42);
    REQUIRE(loc2.column() == 7);

    SourceLocation loc3{1000, 500};
    REQUIRE(loc3.line() == 1000);
    REQUIRE(loc3.column() == 500);
}

TEST_CASE("SourceLocation: adjacent locations differ",
          "[core][location]") {
    SourceLocation a{1, 1};
    SourceLocation b{1, 2};
    SourceLocation c{2, 1};

    REQUIRE(a != b);
    REQUIRE(a != c);
    REQUIRE(b != c);
    REQUIRE(a == SourceLocation{1, 1});
}

// ─────────────────────────────────────────────────────────────────────────────
// Limits
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("SourceLocation: max representable line and column",
          "[core][location][limits]") {
    SourceLocation loc{SourceLocation::kMaxLine,
                       SourceLocation::kMaxColumn};
    REQUIRE(loc.line()   == SourceLocation::kMaxLine);
    REQUIRE(loc.column() == SourceLocation::kMaxColumn);
}

TEST_CASE("SourceLocation: column is clamped, not wrapped",
          "[core][location][limits]") {
    // A column past the limit is stored as the limit. This is the
    // documented behavior: a diagnostic that lands past the limit
    // reads as "far to the right", not as a small column.
    SourceLocation loc{1, SourceLocation::kMaxColumn + 1};
    REQUIRE(loc.line()   == 1);
    REQUIRE(loc.column() == SourceLocation::kMaxColumn);

    SourceLocation loc2{1, 100000};
    REQUIRE(loc2.column() == SourceLocation::kMaxColumn);
}

// ─────────────────────────────────────────────────────────────────────────────
// Streaming
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("SourceLocation: streaming writes line:column",
          "[core][location][stream]") {
    std::ostringstream oss;
    oss << SourceLocation{12, 5};
    REQUIRE(oss.str() == "12:5");
}

TEST_CASE("SourceLocation: streaming an unknown location",
          "[core][location][stream]") {
    std::ostringstream oss;
    oss << SourceLocation{};
    REQUIRE(oss.str() == "<unknown location>");
}