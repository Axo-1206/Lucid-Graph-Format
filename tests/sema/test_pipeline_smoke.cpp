/// @file tests/sema/test_pipeline_smoke.cpp
///
/// @brief A single end-to-end test for the whole compile pipeline.
///
/// ─── What this tests ──────────────────────────────────────────────────────
/// The pipeline runs: parse → resolve → type-check → build graph →
/// dead-code. The individual passes have their own test files. This
/// file runs the whole chain on a few small inputs and checks that the
/// graph has the expected shape. It is a guard against a pass silently
/// doing nothing.

#include "sema/Sema.hpp"
#include "sema/Graph.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace lucid::sema;

namespace
{

    // A minimal registry covering the node types the smoke test uses.
    struct SmokeRegistry
    {
        std::vector<PhaseInfo> phases;
        std::vector<NodeArgInfo> floatArgs;
        std::vector<NodeArgInfo> actionArgs;
        std::vector<NodeTypeInfo> nodeTypes;
        Registry registry;

        SmokeRegistry()
        {
            phases.push_back({"update"});

            floatArgs.push_back({"value",
                                 TypeId::primitive("float32")});
            nodeTypes.push_back({"Float32Node", NodeKind::Value, "Math", 0,
                                 ArenaSpan<NodeArgInfo>(floatArgs.data(), floatArgs.size()),
                                 TypeId::primitive("float32")});

            actionArgs.push_back({"value",
                                  TypeId::primitive("float32")});
            nodeTypes.push_back({"PrintNode", NodeKind::Action, "Debug", 0,
                                 ArenaSpan<NodeArgInfo>(actionArgs.data(),
                                                        actionArgs.size()),
                                 TypeId{}});

            nodeTypes.push_back({"EveryFrame", NodeKind::Trigger, "Flow", 0,
                                 ArenaSpan<NodeArgInfo>{},
                                 TypeId{}});

            registry.phases = ArenaSpan<PhaseInfo>(phases.data(),
                                                   phases.size());
            registry.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes.data(),
                                                         nodeTypes.size());
        }
    };

} // namespace

TEST_CASE("pipeline compiles a single node", "[sema][smoke]")
{
    SmokeRegistry reg;
    auto result = compile("node x = Float32Node(1.5)\n",
                          "main.lucid", reg.registry);

    REQUIRE(result.ok);
    REQUIRE(result.graph != nullptr);
    CHECK(result.graph->nodes.size() == 1);
    CHECK(result.graph->phase_order.empty());
    CHECK(result.graph->value_order.size() == 1);
}

TEST_CASE("pipeline compiles a value chain", "[sema][smoke]")
{
    SmokeRegistry reg;
    auto result = compile(
        "node a = Float32Node(1.0)\n"
        "node b = Float32Node(a)\n"
        "node tick = EveryFrame()\n"
        "node print = PrintNode(b) on tick\n",
        "main.lucid", reg.registry);

    REQUIRE(result.ok);
    REQUIRE(result.graph != nullptr);
    CHECK(result.graph->nodes.size() == 4);
    CHECK(result.graph->value_order.size() == 2); // a, b
    CHECK(result.graph->phase_order.size() == 1); // print
}

TEST_CASE("pipeline produces an empty graph on an empty module",
          "[sema][smoke]")
{
    SmokeRegistry reg;
    auto result = compile("", "main.lucid", reg.registry);

    REQUIRE(result.ok);
    REQUIRE(result.graph != nullptr);
    CHECK(result.graph->nodes.empty());
    CHECK(result.graph->resources.empty());
    CHECK(result.graph->phase_order.empty());
    CHECK(result.graph->value_order.empty());
}

TEST_CASE("pipeline reports a syntax error", "[sema][smoke]")
{
    SmokeRegistry reg;
    auto result = compile("node = Float32Node(1.5)\n",
                          "main.lucid", reg.registry);

    CHECK_FALSE(result.ok);
    CHECK(result.graph == nullptr);
    CHECK_FALSE(result.diagnostics.empty());
}

TEST_CASE("pipeline reports a semantic error", "[sema][smoke]")
{
    SmokeRegistry reg;
    auto result = compile("node x = NoSuchNode()\n",
                          "main.lucid", reg.registry);

    CHECK_FALSE(result.ok);
    CHECK_FALSE(result.diagnostics.empty());
}

TEST_CASE("pipeline reports a trigger rule violation", "[sema][smoke]")
{
    SmokeRegistry reg;
    auto result = compile("node p = PrintNode(1.0)\n",
                          "main.lucid", reg.registry);

    CHECK_FALSE(result.ok);

    bool found = false;
    for (const auto &d : result.diagnostics)
    {
        if (d.code == lucid::diag::DiagCode::Trigger_ActionWithoutOn)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("pipeline emits a dead-code warning without failing",
          "[sema][smoke]")
{
    SmokeRegistry reg;
    auto result = compile("node unused = Float32Node(1.5)\n",
                          "main.lucid", reg.registry);

    // The compile succeeds; the warning is a warning, not an error.
    REQUIRE(result.ok);
    REQUIRE(result.graph != nullptr);

    bool found = false;
    for (const auto &d : result.diagnostics)
    {
        if (d.code == lucid::diag::DiagCode::Warn_DeadNode)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}