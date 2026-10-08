/// @file tests/sema/test_resolution_map.cpp
///
/// @brief Tests for ResolutionMap.

#include "sema/ResolutionMap.hpp"

#include <catch2/catch_test_macros.hpp>

using lucid::sema::ResolutionMap;

namespace
{

    // Dummy nodes for testing. The map only uses their addresses.
    int ref1;
    int ref2;
    int target1;

} // namespace

TEST_CASE("ResolutionMap is empty by default", "[sema][resolution-map]")
{
    ResolutionMap map;
    CHECK(map.empty());
    CHECK(map.size() == 0);
}

TEST_CASE("ResolutionMap records and looks up a resolution",
          "[sema][resolution-map]")
{
    ResolutionMap map;

    const auto* ref = reinterpret_cast<const BaseAST*>(&ref1);
    const auto* target = reinterpret_cast<const BaseAST*>(&target1);

    map.record(ref, target);

    CHECK(map.size() == 1);
    CHECK(map.contains(ref));
    CHECK(map.lookup(ref) == target);
}

TEST_CASE("ResolutionMap returns nullptr for an unrecorded reference",
          "[sema][resolution-map]")
{
    ResolutionMap map;
    const auto* ref = reinterpret_cast<const BaseAST*>(&ref1);

    CHECK_FALSE(map.contains(ref));
    CHECK(map.lookup(ref) == nullptr);
}

TEST_CASE("ResolutionMap distinguishes a deferred null from not recorded",
          "[sema][resolution-map]")
{
    ResolutionMap map;
    const auto* ref = reinterpret_cast<const BaseAST*>(&ref1);

    map.record(ref, nullptr);

    CHECK(map.contains(ref));      // recorded
    CHECK(map.lookup(ref) == nullptr);   // but with a null target
}

TEST_CASE("ResolutionMap overwrites a record", "[sema][resolution-map]")
{
    ResolutionMap map;
    const auto* ref = reinterpret_cast<const BaseAST*>(&ref1);
    const auto* target = reinterpret_cast<const BaseAST*>(&target1);

    map.record(ref, nullptr);
    CHECK(map.lookup(ref) == nullptr);

    map.record(ref, target);
    CHECK(map.lookup(ref) == target);
    CHECK(map.size() == 1);
}
