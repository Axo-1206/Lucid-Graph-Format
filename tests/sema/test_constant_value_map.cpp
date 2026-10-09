/// @file tests/sema/test_constant_value_map.cpp
///
/// @brief Tests for ConstantValueMap.

#include "sema/ConstantValueMap.hpp"

#include <catch2/catch_test_macros.hpp>

using lucid::sema::ConstantValueMap;
using lucid::sema::Literal;

namespace
{
    int ref;
} // dummy address

TEST_CASE("ConstantValueMap is empty by default",
          "[sema][constant-value-map]")
{
    ConstantValueMap map;
    CHECK(map.empty());
    CHECK(map.size() == 0);
}

TEST_CASE("ConstantValueMap records and looks up a value",
          "[sema][constant-value-map]")
{
    ConstantValueMap map;
    const auto *node = reinterpret_cast<const BaseAST *>(&ref);

    Literal lit;
    lit.kind = Literal::Kind::Int32;
    lit.i = 42;

    map.record(node, lit);

    CHECK(map.size() == 1);
    CHECK(map.contains(node));

    const Literal *found = map.lookup(node);
    REQUIRE(found != nullptr);
    CHECK(found->kind == Literal::Kind::Int32);
    CHECK(found->i == 42);
}

TEST_CASE("ConstantValueMap returns nullptr for an unrecorded node",
          "[sema][constant-value-map]")
{
    ConstantValueMap map;
    const auto *node = reinterpret_cast<const BaseAST *>(&ref);

    CHECK_FALSE(map.contains(node));
    CHECK(map.lookup(node) == nullptr);
}