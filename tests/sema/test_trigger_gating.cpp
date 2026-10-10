/// @file tests/sema/test_trigger_gating.cpp
///
/// @brief Tests for the trigger subscription model.
///
/// ─── What this tests ──────────────────────────────────────────────────────
/// Three patterns the language permits:
///
///   1. An Action with multiple `on` clauses — it subscribes to every
///      listed trigger.
///   2. A Trigger with an `on` clause — a "gating trigger". Sema accepts
///      it; the engine gives it meaning.
///   3. The Sema errors that still apply:
///      - an Action with no `on` (Trigger_ActionWithoutOn)
///      - an `on` target that is not a Trigger (Trigger_OnTargetNotTrigger)
///
/// ─── What this does not test ──────────────────────────────────────────────
/// The *runtime* semantics of gating. That is the engine's job; the
/// library only stores the subscription. This file asserts that the
/// subscription is stored correctly.

#include "sema/Sema.hpp"
#include "sema/Graph.hpp"
#include "sema/Registry.hpp"
#include "sema/NodeKind.hpp"
#include "sema/TypeId.hpp"
#include "core/memory/ArenaSpan.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <string_view>

using namespace lucid::sema;

namespace
{

    // ─── A registry with the node types these tests need ─────────────────
    //
    // Two triggers (EveryFrameNode, OnHitNode), one gating trigger
    // (IfNode), two actions (PrintNode, DamageNode), and one value
    // node (Float32Node). Enough to write every pattern under test.

    const Registry &testRegistry()
    {
        static const PhaseInfo phases[] = {
            {"update"},
        };

        static const NodeArgInfo float32Args[] = {
            {"value", TypeId::primitive("float32")},
        };

        static const NodeArgInfo ifArgs[] = {
            {"condition", TypeId::primitive("bool")},
        };

        static const NodeArgInfo damageArgs[] = {
            {"amount", TypeId::primitive("int32")},
        };

        static const NodeTypeInfo nodeTypes[] = {
            // Index 0
            {"Float32Node", NodeKind::Value, "Math", 0,
             ArenaSpan<NodeArgInfo>(float32Args, 1),
             TypeId::primitive("float32")},

            // Index 1
            {"BoolNode", NodeKind::Value, "Math", 0,
             ArenaSpan<NodeArgInfo>{},
             TypeId::primitive("bool")},

            // Index 2
            {"PrintNode", NodeKind::Action, "Debug", 0,
             ArenaSpan<NodeArgInfo>(float32Args, 1),
             TypeId{}},

            // Index 3
            {"DamageNode", NodeKind::Action, "Physics", 0,
             ArenaSpan<NodeArgInfo>(damageArgs, 1),
             TypeId{}},

            // Index 4
            {"EveryFrameNode", NodeKind::Trigger, "Flow", 0,
             ArenaSpan<NodeArgInfo>{},
             TypeId{}},

            // Index 5
            {"OnHitNode", NodeKind::Trigger, "Flow", 0,
             ArenaSpan<NodeArgInfo>{},
             TypeId{}},

            // Index 6
            {"IfNode", NodeKind::Trigger, "Flow", 0,
             ArenaSpan<NodeArgInfo>(ifArgs, 1),
             TypeId{}},
        };

        static const Registry reg = []
        {
            Registry r;
            r.phases = ArenaSpan<PhaseInfo>(phases, 1);
            r.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes, 7);
            return r;
        }();

        return reg;
    }

    /// Compile `source` against the test registry. Fails the test on
    /// error, printing the diagnostics.
    std::unique_ptr<Graph> compileOrFail(std::string_view source)
    {
        auto result = compile(source, "test.lucid", testRegistry());
        if (!result.ok)
        {
            std::string joined;
            for (const auto &d : result.diagnostics)
            {
                joined += "  ";
                joined += d.message;
                joined += "\n";
            }
            FAIL("compile failed:\n"
                 << joined);
        }
        return std::move(result.graph);
    }

    /// Compile `source` expecting failure. Returns the diagnostics.
    std::vector<lucid::diag::Diagnostic>
    compileExpectingFailure(std::string_view source)
    {
        auto result = compile(source, "test.lucid", testRegistry());
        REQUIRE_FALSE(result.ok);
        return result.diagnostics;
    }

    /// True if `diagnostics` contains `code`.
    bool hasCode(const std::vector<lucid::diag::Diagnostic> &diags,
                 lucid::diag::DiagCode code)
    {
        for (const auto &d : diags)
        {
            if (d.code == code)
                return true;
        }
        return false;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// An Action with multiple `on` clauses
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("sema: an action can subscribe to multiple triggers",
          "[sema][gating]")
{
    constexpr std::string_view source = R"(
        node every_frame = EveryFrameNode()
        node on_hit      = OnHitNode()
        node damage      = DamageNode(10) on every_frame, on_hit
    )";

    auto graph = compileOrFail(source);
    REQUIRE(graph != nullptr);
    REQUIRE(graph->nodes.size() == 3);

    // The graph's nodes, in source order:
    //   0: every_frame
    //   1: on_hit
    //   2: damage
    //
    // `damage` subscribes to two triggers; the graph's `subscribers`
    // vector holds the reverse direction: every_frame's subscriber
    // list contains `damage`; on_hit's subscriber list contains
    // `damage`.

    const NodeInstance &everyFrame = graph->nodes[0];
    const NodeInstance &onHit = graph->nodes[1];
    const NodeInstance &damage = graph->nodes[2];

    // EveryFrame has one subscriber: damage.
    {
        const auto subs = graph->subscribersOf(everyFrame);
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 2);
    }

    // OnHit has one subscriber: damage.
    {
        const auto subs = graph->subscribersOf(onHit);
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 2);
    }

    // Damage subscribes to both; its own subscriber list is empty
    // (it's an Action, so nothing subscribes to it).
    {
        const auto subs = graph->subscribersOf(damage);
        CHECK(subs.size() == 0);
    }
}

TEST_CASE("sema: an action with no `on` is rejected",
          "[sema][gating]")
{
    constexpr std::string_view source = R"(
        node damage = DamageNode(10)
    )";

    const auto diags = compileExpectingFailure(source);
    CHECK(hasCode(diags, lucid::diag::DiagCode::Trigger_ActionWithoutOn));
}

TEST_CASE("sema: an `on` target that is not a trigger is rejected",
          "[sema][gating]")
{
    constexpr std::string_view source = R"(
        node value  = Float32Node(1.0)
        node damage = DamageNode(10) on value
    )";

    const auto diags = compileExpectingFailure(source);
    CHECK(hasCode(diags, lucid::diag::DiagCode::Trigger_OnTargetNotTrigger));
}

// ─────────────────────────────────────────────────────────────────────────────
// A Trigger with an `on` clause (the gating pattern)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("sema: a trigger may have an `on` clause (gating trigger)",
          "[sema][gating]")
{
    // The gating pattern:
    //   every_frame is a source trigger.
    //   condition is a value.
    //   if_node is a trigger with an `on` clause; it fires only when
    //     every_frame fires AND its condition is true (semantics
    //     belong to the engine; the library only stores the
    //     subscription).
    //   effect is an action that runs when if_node fires.
    constexpr std::string_view source = R"(
        node every_frame = EveryFrameNode()
        node condition   = BoolNode()
        node if_node     = IfNode(condition) on every_frame
        node effect      = PrintNode(Float32Node(1.0)) on if_node
    )";

    auto graph = compileOrFail(source);
    REQUIRE(graph != nullptr);

    // Source declares four nodes (every_frame, condition, if_node,
    // effect). The inline `Float32Node(1.0)` inside `PrintNode(...)`
    // becomes a fifth node — the graph builder appends it after all
    // declared nodes. The declared nodes keep their source-order
    // indices 0..3.
    REQUIRE(graph->nodes.size() == 5);

    // Node indices:
    //   0: every_frame
    //   1: condition
    //   2: if_node
    //   3: effect
    //   4: <anonymous Float32Node(1.0)>
    //
    // Subscriptions:
    //   every_frame → {if_node}
    //   if_node     → {effect}

    {
        const auto subs = graph->subscribersOf(graph->nodes[0]);
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 2); // if_node
    }

    {
        const auto subs = graph->subscribersOf(graph->nodes[2]);
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 3); // effect
    }
}

TEST_CASE("sema: gating triggers compose",
          "[sema][gating]")
{
    // Two levels of gating. EveryFrame fires if_a; if_a fires if_b;
    // if_b fires the effect. The library stores the subscriptions;
    // whether the chain fires is the engine's decision.
    constexpr std::string_view source = R"(
        node every_frame = EveryFrameNode()
        node c1          = BoolNode()
        node c2          = BoolNode()
        node if_a        = IfNode(c1) on every_frame
        node if_b        = IfNode(c2) on if_a
        node effect      = PrintNode(Float32Node(1.0)) on if_b
    )";

    auto graph = compileOrFail(source);
    REQUIRE(graph != nullptr);

    // Source declares six nodes. The inline `Float32Node(1.0)` inside
    // `PrintNode(...)` becomes a seventh. Declared nodes keep their
    // source-order indices 0..5.
    REQUIRE(graph->nodes.size() == 7);

    // Node indices:
    //   0: every_frame
    //   1: c1
    //   2: c2
    //   3: if_a
    //   4: if_b
    //   5: effect
    //   6: <anonymous Float32Node(1.0)>
    //
    // Subscriptions:
    //   every_frame (0) → {if_a (3)}
    //   if_a        (3) → {if_b (4)}
    //   if_b        (4) → {effect (5)}

    {
        const auto subs = graph->subscribersOf(graph->nodes[0]);
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 3);
    }

    {
        const auto subs = graph->subscribersOf(graph->nodes[3]);
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 4);
    }

    {
        const auto subs = graph->subscribersOf(graph->nodes[4]);
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 5);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// A gating trigger with multiple `on` clauses
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("sema: a gating trigger can subscribe to multiple triggers",
          "[sema][gating]")
{
    constexpr std::string_view source = R"(
        node every_frame = EveryFrameNode()
        node on_hit      = OnHitNode()
        node condition   = BoolNode()
        node if_node     = IfNode(condition) on every_frame, on_hit
        node effect      = PrintNode(Float32Node(1.0)) on if_node
    )";

    auto graph = compileOrFail(source);
    REQUIRE(graph != nullptr);

    // Source declares five nodes. The inline `Float32Node(1.0)`
    // inside `PrintNode(...)` becomes a sixth. Declared nodes keep
    // their source-order indices 0..4.
    REQUIRE(graph->nodes.size() == 6);

    // Node indices:
    //   0: every_frame
    //   1: on_hit
    //   2: condition
    //   3: if_node
    //   4: effect
    //   5: <anonymous Float32Node(1.0)>
    //
    // Subscriptions:
    //   every_frame (0) → {if_node (3)}
    //   on_hit      (1) → {if_node (3)}
    //   if_node     (3) → {effect (4)}

    {
        const auto subs = graph->subscribersOf(graph->nodes[0]);
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 3);
    }

    {
        const auto subs = graph->subscribersOf(graph->nodes[1]);
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 3);
    }

    {
        const auto subs = graph->subscribersOf(graph->nodes[3]);
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 4);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Circle test
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("sema: a subscription cycle is accepted",
          "[sema][gating]")
{
    // a and b subscribe to each other. The library accepts it;
    // the cycle is broken at runtime by the engine's fired-this-
    // tick rule.
    constexpr std::string_view source = R"(
        node c1      = BoolNode()
        node c2      = BoolNode()
        node a       = IfNode(c1) on b
        node b       = IfNode(c2) on a
    )";

    auto graph = compileOrFail(source);
    REQUIRE(graph != nullptr);
    REQUIRE(graph->nodes.size() == 4);

    // a (2) subscribes to b (3); b (3) subscribes to a (2).
    {
        const auto subs = graph->subscribersOf(graph->nodes[3]); // b
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 2); // a
    }
    {
        const auto subs = graph->subscribersOf(graph->nodes[2]); // a
        REQUIRE(subs.size() == 1);
        CHECK(subs[0] == 3); // b
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Inline node expression
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("sema: an inline node expression becomes its own graph node",
          "[sema][gating]")
{
    constexpr std::string_view source = R"(
        node every_frame = EveryFrameNode()
        node effect = PrintNode(Float32Node(1.0)) on every_frame
    )";

    auto graph = compileOrFail(source);
    REQUIRE(graph->nodes.size() == 3); // every_frame, effect, <anonymous>

    // The last node is the anonymous Float32Node.
    const NodeInstance &anon = graph->nodes[2];
    CHECK(anon.type_id == 0); // Float32Node
    CHECK(anon.args_count == 1);

    // `effect`'s argument is a NodeRef to the anonymous node.
    const auto args = graph->argsOf(graph->nodes[1]);
    REQUIRE(args.size() == 1);
    CHECK(args[0].kind == Arg::Kind::NodeRef);
    CHECK(args[0].node_ref == 2);
}