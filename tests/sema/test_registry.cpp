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

    const NodeArgInfo addArgs[] = {
        {"a", TypeId::primitive("float32")},
        {"b", TypeId::primitive("float32")},
    };
    const NodeTypeInfo nodeTypes[] = {
        {
            "AddNode",
            NodeKind::Value,
            "Math",
            0,
            ArenaSpan<NodeArgInfo>(addArgs, 2),
            TypeId::primitive("float32"),
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
    CHECK(reg.nodeTypes[0].args.size() == 2);
    CHECK(reg.nodeTypes[0].args[0].name == "a");
    CHECK(reg.nodeTypes[0].args[0].type == TypeId::primitive("float32"));
    CHECK(reg.nodeTypes[0].resultType == TypeId::primitive("float32"));
}

TEST_CASE("An action node's resultType is invalid", "[sema][registry]")
{
    // Action nodes perform a side effect; they produce no value. Their
    // resultType is left default-constructed (Kind::Invalid).
    const NodeArgInfo bodyArgs[] = {
        {"body", TypeId::handle("BodyRef")},
    };
    const NodeTypeInfo nodeTypes[] = {
        {
            "MoveBody",
            NodeKind::Action,
            "Physics",
            0,
            ArenaSpan<NodeArgInfo>(bodyArgs, 1),
            TypeId{},
        },
    };

    Registry reg;
    reg.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes, 1);

    CHECK(reg.nodeTypes[0].kind == NodeKind::Action);
    CHECK_FALSE(reg.nodeTypes[0].resultType.isValid());
}

TEST_CASE("An empty NodeTypeInfo has no args and an invalid result",
          "[sema][registry]")
{
    NodeTypeInfo info{};
    CHECK(info.args.empty());
    CHECK_FALSE(info.resultType.isValid());
}

TEST_CASE("NodeTypeInfo default phase is zero", "[sema][registry]")
{
    NodeTypeInfo info{};
    CHECK(info.phase == 0);
}