/// @file tests/sema/test_registry.cpp
///
/// @brief Tests for the Registry data types.
#include "sema/Registry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string_view>

using namespace lucid::sema;

TEST_CASE("Registry is empty by default", "[sema][registry]")
{
    Registry reg;
    CHECK(reg.phases.empty());
    CHECK(reg.enums.empty());
    CHECK(reg.handles.empty());
    CHECK(reg.nodeTypes.empty());
}

TEST_CASE("Registry holds spans of the engine's data", "[sema][registry]")
{
    const PhaseInfo phases[] = {
        {"init"},
        {"update"},
    };

    const NodePortInfo addInputs[] = {
        {"a", TypeId::primitive("float32")},
        {"b", TypeId::primitive("float32")},
    };
    const NodePortInfo addOutputs[] = {
        {"result", TypeId::primitive("float32")},
    };
    const NodeTypeInfo nodeTypes[] = {
        {
            "AddNode",
            NodeKind::Value,
            "Math",
            0,
            ArenaSpan<NodePortInfo>(addInputs, 2),
            ArenaSpan<NodePortInfo>(addOutputs, 1),
            ArenaSpan<NodePortInfo>{},
        },
    };

    Registry reg;
    reg.phases = ArenaSpan<PhaseInfo>(phases, 2);
    reg.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes, 1);

    CHECK(reg.phases.size() == 2);
    CHECK(reg.nodeTypes.size() == 1);
    CHECK(reg.nodeTypes[0].name == "AddNode");
    CHECK(reg.nodeTypes[0].kind == NodeKind::Value);
    CHECK(reg.nodeTypes[0].category == "Math");
    CHECK(reg.nodeTypes[0].inputs.size() == 2);
    CHECK(reg.nodeTypes[0].outputs.size() == 1);
    CHECK(reg.nodeTypes[0].payload.empty());
}

TEST_CASE("NodeTypeInfo default phase is zero", "[sema][registry]")
{
    NodeTypeInfo info{};
    CHECK(info.phase == 0);
}
