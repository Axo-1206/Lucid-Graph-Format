/// @file tests/sema/test_node_kind.cpp
///
/// @brief Tests for NodeKind.
#include "sema/NodeKind.hpp"

#include <catch2/catch_test_macros.hpp>

using lucid::sema::NodeKind;
using lucid::sema::nodeKindName;

TEST_CASE("NodeKind has exactly three values", "[sema][node-kind]")
{
    // If a fourth value is added, this test fails and reminds the
    // developer to update the name function and every switch over
    // NodeKind.
    CHECK(nodeKindName(NodeKind::Value)   != nullptr);
    CHECK(nodeKindName(NodeKind::Action)  != nullptr);
    CHECK(nodeKindName(NodeKind::Trigger) != nullptr);
}

TEST_CASE("NodeKind names are stable", "[sema][node-kind]")
{
    CHECK(std::string_view(nodeKindName(NodeKind::Value))   == "Value");
    CHECK(std::string_view(nodeKindName(NodeKind::Action))  == "Action");
    CHECK(std::string_view(nodeKindName(NodeKind::Trigger)) == "Trigger");
}
