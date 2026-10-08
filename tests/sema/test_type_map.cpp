/// @file tests/sema/test_type_map.cpp
///
/// @brief Tests for TypeMap.

#include "sema/TypeMap.hpp"

#include <catch2/catch_test_macros.hpp>

using lucid::sema::TypeId;
using lucid::sema::TypeMap;

namespace
{
    int ref;
} // dummy address

TEST_CASE("TypeMap is empty by default", "[sema][type-map]")
{
    TypeMap map;
    CHECK(map.empty());
    CHECK(map.size() == 0);
}

TEST_CASE("TypeMap records and looks up a type", "[sema][type-map]")
{
    TypeMap map;
    const auto *node = reinterpret_cast<const BaseAST *>(&ref);
    const TypeId t = TypeId::primitive("float32");

    map.record(node, t);

    CHECK(map.size() == 1);
    CHECK(map.contains(node));
    CHECK(map.lookup(node) == t);
}

TEST_CASE("TypeMap returns invalid for an unrecorded node",
          "[sema][type-map]")
{
    TypeMap map;
    const auto *node = reinterpret_cast<const BaseAST *>(&ref);

    CHECK_FALSE(map.contains(node));
    CHECK_FALSE(map.lookup(node).isValid());
}

TEST_CASE("TypeMap overwrites a record", "[sema][type-map]")
{
    TypeMap map;
    const auto *node = reinterpret_cast<const BaseAST *>(&ref);

    map.record(node, TypeId::primitive("float32"));
    map.record(node, TypeId::primitive("float64"));

    CHECK(map.lookup(node) == TypeId::primitive("float64"));
    CHECK(map.size() == 1);
}
