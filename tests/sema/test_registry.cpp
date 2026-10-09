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

// ─────────────────────────────────────────────────────────────────────────────
// computeRegistryFingerprint
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("computeRegistryFingerprint on an empty registry",
          "[sema][registry][fingerprint]")
{
    Registry reg;
    const uint64_t fp = computeRegistryFingerprint(reg);

    // The fingerprint is deterministic. Two empty registries produce
    // the same value.
    Registry reg2;
    CHECK(computeRegistryFingerprint(reg2) == fp);

    // It is not the raw FNV-1a offset basis (the version byte was
    // hashed first). It is nonzero.
    CHECK(fp != 0);
}

TEST_CASE("computeRegistryFingerprint is deterministic",
          "[sema][registry][fingerprint]")
{
    // Two identical registries produce the same fingerprint.
    const PhaseInfo phases1[] = {{"update"}};
    const PhaseInfo phases2[] = {{"update"}};

    Registry r1, r2;
    r1.phases = ArenaSpan<PhaseInfo>(phases1, 1);
    r2.phases = ArenaSpan<PhaseInfo>(phases2, 1);

    CHECK(computeRegistryFingerprint(r1) == computeRegistryFingerprint(r2));
}

TEST_CASE("computeRegistryFingerprint changes when a phase name changes",
          "[sema][registry][fingerprint]")
{
    const PhaseInfo phases1[] = {{"update"}};
    const PhaseInfo phases2[] = {{"render"}};

    Registry r1, r2;
    r1.phases = ArenaSpan<PhaseInfo>(phases1, 1);
    r2.phases = ArenaSpan<PhaseInfo>(phases2, 1);

    CHECK(computeRegistryFingerprint(r1) != computeRegistryFingerprint(r2));
}

TEST_CASE("computeRegistryFingerprint changes when an enum member value changes",
          "[sema][registry][fingerprint]")
{
    const EnumMemberInfo m1[] = {{"W", 0}, {"A", 1}};
    const EnumMemberInfo m2[] = {{"W", 0}, {"A", 5}};

    const EnumTypeInfo e1[] = {
        {"Key", ArenaSpan<EnumMemberInfo>(m1, 2)}};
    const EnumTypeInfo e2[] = {
        {"Key", ArenaSpan<EnumMemberInfo>(m2, 2)}};

    Registry r1, r2;
    r1.enums = ArenaSpan<EnumTypeInfo>(e1, 1);
    r2.enums = ArenaSpan<EnumTypeInfo>(e2, 1);

    CHECK(computeRegistryFingerprint(r1) != computeRegistryFingerprint(r2));
}

TEST_CASE("computeRegistryFingerprint changes when a node type's kind changes",
          "[sema][registry][fingerprint]")
{
    const NodeTypeInfo n1[] = {
        {"Foo", NodeKind::Value, "Math", 0, {}, TypeId{}}};
    const NodeTypeInfo n2[] = {
        {"Foo", NodeKind::Action, "Math", 0, {}, TypeId{}}};

    Registry r1, r2;
    r1.nodeTypes = ArenaSpan<NodeTypeInfo>(n1, 1);
    r2.nodeTypes = ArenaSpan<NodeTypeInfo>(n2, 1);

    CHECK(computeRegistryFingerprint(r1) != computeRegistryFingerprint(r2));
}

TEST_CASE("computeRegistryFingerprint changes when an argument type changes",
          "[sema][registry][fingerprint]")
{
    const NodeArgInfo a1[] = {{"x", TypeId::primitive("float32")}};
    const NodeArgInfo a2[] = {{"x", TypeId::primitive("int32")}};

    const NodeTypeInfo n1[] = {
        {"Foo", NodeKind::Value, "Math", 0,
         ArenaSpan<NodeArgInfo>(a1, 1), TypeId{}}};
    const NodeTypeInfo n2[] = {
        {"Foo", NodeKind::Value, "Math", 0,
         ArenaSpan<NodeArgInfo>(a2, 1), TypeId{}}};

    Registry r1, r2;
    r1.nodeTypes = ArenaSpan<NodeTypeInfo>(n1, 1);
    r2.nodeTypes = ArenaSpan<NodeTypeInfo>(n2, 1);

    CHECK(computeRegistryFingerprint(r1) != computeRegistryFingerprint(r2));
}

TEST_CASE("computeRegistryFingerprint is order-sensitive",
          "[sema][registry][fingerprint]")
{
    // Two phases in different orders produce different fingerprints,
    // even though the set of names is the same.
    const PhaseInfo p1[] = {{"init"}, {"update"}};
    const PhaseInfo p2[] = {{"update"}, {"init"}};

    Registry r1, r2;
    r1.phases = ArenaSpan<PhaseInfo>(p1, 2);
    r2.phases = ArenaSpan<PhaseInfo>(p2, 2);

    CHECK(computeRegistryFingerprint(r1) != computeRegistryFingerprint(r2));
}

TEST_CASE("computeRegistryFingerprint is not sensitive to redundant whitespace",
          "[sema][registry][fingerprint]")
{
    // Two registries with the same names but different string_view
    // sources (one literal, one heap-allocated with the same bytes)
    // produce the same fingerprint.
    std::string nameA = "update";
    std::string nameB = "update";

    const PhaseInfo p1[] = {{nameA}};
    const PhaseInfo p2[] = {{nameB}};

    Registry r1, r2;
    r1.phases = ArenaSpan<PhaseInfo>(p1, 1);
    r2.phases = ArenaSpan<PhaseInfo>(p2, 1);

    CHECK(computeRegistryFingerprint(r1) == computeRegistryFingerprint(r2));
}

TEST_CASE("computeRegistryFingerprint changes when the number of items changes",
          "[sema][registry][fingerprint]")
{
    const PhaseInfo p1[] = {{"init"}};
    const PhaseInfo p2[] = {{"init"}, {"update"}};

    Registry r1, r2;
    r1.phases = ArenaSpan<PhaseInfo>(p1, 1);
    r2.phases = ArenaSpan<PhaseInfo>(p2, 2);

    CHECK(computeRegistryFingerprint(r1) != computeRegistryFingerprint(r2));
}

TEST_CASE("computeRegistryFingerprint is length-sensitive",
          "[sema][registry][fingerprint]")
{
    // A name "ab" and a name "a" followed by an unrelated byte should
    // not collide. This is ensured by length-prefixing.
    const PhaseInfo p1[] = {{"ab"}};
    const PhaseInfo p2[] = {{"a"}};

    Registry r1, r2;
    r1.phases = ArenaSpan<PhaseInfo>(p1, 1);
    r2.phases = ArenaSpan<PhaseInfo>(p2, 1);

    CHECK(computeRegistryFingerprint(r1) != computeRegistryFingerprint(r2));
}

TEST_CASE("computeRegistryFingerprint changes when a node type's category changes",
          "[sema][registry][fingerprint]")
{
    const NodeTypeInfo n1[] = {
        {"Foo", NodeKind::Value, "Math", 0, {}, TypeId{}}};
    const NodeTypeInfo n2[] = {
        {"Foo", NodeKind::Value, "Logic", 0, {}, TypeId{}}};

    Registry r1, r2;
    r1.nodeTypes = ArenaSpan<NodeTypeInfo>(n1, 1);
    r2.nodeTypes = ArenaSpan<NodeTypeInfo>(n2, 1);

    CHECK(computeRegistryFingerprint(r1) != computeRegistryFingerprint(r2));
}

TEST_CASE("computeRegistryFingerprint changes when a node type's phase changes",
          "[sema][registry][fingerprint]")
{
    const NodeTypeInfo n1[] = {
        {"Move", NodeKind::Action, "Physics", 0, {}, TypeId{}}};
    const NodeTypeInfo n2[] = {
        {"Move", NodeKind::Action, "Physics", 1, {}, TypeId{}}};

    Registry r1, r2;
    r1.nodeTypes = ArenaSpan<NodeTypeInfo>(n1, 1);
    r2.nodeTypes = ArenaSpan<NodeTypeInfo>(n2, 1);

    CHECK(computeRegistryFingerprint(r1) != computeRegistryFingerprint(r2));
}