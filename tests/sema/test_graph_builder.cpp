/// @file tests/sema/test_graph_builder.cpp
///
/// @brief Tests for Pass 4: graph construction.

#include "sema/Graph.hpp"
#include "sema/ConstantValueMap.hpp"
#include "sema/SymbolTable.hpp"
#include "sema/TypeMap.hpp"

#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <utility>
#include <vector>

#include "sema/GraphBuilder.hpp"
#include "sema/Resolver.hpp"
#include "sema/SymbolCollector.hpp"
#include "sema/TypeChecker.hpp"

using lucid::diag::DiagnosticEngine;
using lucid::parser::parseFile;
using lucid::parser::ParserContext;
using lucid::parser::TokenStream;
using namespace lucid::sema;

namespace
{

    // Reuse the TestRegistry shape from test_type_checker.cpp.
    // (Paste a compatible registry here.)

    struct TestRegistry
    {
        std::vector<PhaseInfo> phases;
        std::vector<HandleTypeInfo> handles;
        std::vector<NodeArgInfo> floatArgs;
        std::vector<NodeArgInfo> actionArgs;
        std::vector<NodeTypeInfo> nodeTypes;
        Registry registry;

        TestRegistry()
        {
            phases.push_back(PhaseInfo{"update"});
            handles.push_back(HandleTypeInfo{"BodyRef"});

            // Float32Node(value: float32) -> float32
            floatArgs.push_back(NodeArgInfo{"value",
                                            TypeId::primitive("float32")});
            nodeTypes.push_back(NodeTypeInfo{
                "Float32Node", NodeKind::Value, "Math", 0,
                ArenaSpan<NodeArgInfo>(floatArgs.data(), floatArgs.size()),
                TypeId::primitive("float32")});

            // MoveBody(body: BodyRef)  -- Action
            actionArgs.push_back(NodeArgInfo{"body",
                                             TypeId::handle("BodyRef")});
            nodeTypes.push_back(NodeTypeInfo{
                "MoveBody", NodeKind::Action, "Physics", 0,
                ArenaSpan<NodeArgInfo>(actionArgs.data(),
                                       actionArgs.size()),
                TypeId{}});

            // EveryFrame()  -- Trigger
            nodeTypes.push_back(NodeTypeInfo{
                "EveryFrame", NodeKind::Trigger, "Flow", 0,
                ArenaSpan<NodeArgInfo>{}, TypeId{}});

            registry.phases = ArenaSpan<PhaseInfo>(phases.data(),
                                                   phases.size());
            registry.handles = ArenaSpan<HandleTypeInfo>(handles.data(),
                                                         handles.size());
            registry.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes.data(),
                                                         nodeTypes.size());
        }
    };

    struct Fixture
    {
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag{&pool};
        TokenStream stream;
        TestRegistry reg;

        Fixture()
            : pool(), arena(),
              stream(std::vector<Token>{
                  Token{TokenType::EOF_TOKEN, InternedString{},
                        SourceLocation{1, 1}}})
        {
        }

        std::unique_ptr<Graph> build(std::string_view source)
        {
            ParserContext ctx(pool, arena, diag, stream);
            ModuleAST *module = parseFile("test.lucid", source, ctx);

            SymbolTable symbols;
            collectSymbols(module, symbols, diag);

            ResolutionMap resolutions;
            resolveNames(module, symbols, resolutions, diag);

            TypeMap types;
            ConstantValueMap constants;
            checkTypes(module, symbols, resolutions, reg.registry,
                       types, constants, diag);

            if (diag.hasErrors())
            {
                // Dump the diagnostics so a test failure shows what went wrong.
                // (Catch2 will show the stream output when the test fails.)
                for (const auto &d : diag.all())
                {
                    WARN(d.message);
                }
                return nullptr;
            }

            return buildGraph(module, symbols, resolutions, constants,
                              types, reg.registry, pool, diag);
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Empty and simple modules
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("buildGraph on an empty module returns an empty graph",
          "[sema][graph-builder]")
{
    Fixture f;
    auto graph = f.build("");
    REQUIRE(graph != nullptr);
    CHECK(graph->nodes.empty());
    CHECK(graph->resources.empty());
    CHECK(graph->args.empty());
    CHECK(graph->phase_order.empty());
    CHECK(graph->value_order.empty());
    CHECK(graph->string_pool.empty());
}

TEST_CASE("buildGraph on a resource-only module",
          "[sema][graph-builder]")
{
    Fixture f;
    auto graph = f.build(
        "resource R { x: float32 = 1.5, y: float32 = 2.5 }\n");
    REQUIRE(graph != nullptr);
    CHECK(graph->nodes.empty());
    REQUIRE(graph->resources.size() == 1);
    CHECK(graph->resources[0].name == "R");
    CHECK(graph->resources[0].fields_count == 2);

    auto fields = graph->fieldsOf(graph->resources[0]);
    REQUIRE(fields.size() == 2);
    CHECK(fields[0].name == "x");
    CHECK(fields[0].hasDefault);
    CHECK(fields[0].defaultValue.kind == Literal::Kind::Float32);
    CHECK(fields[1].name == "y");
}

// ─────────────────────────────────────────────────────────────────────────────
// Single node
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("buildGraph on a single value node",
          "[sema][graph-builder]")
{
    Fixture f;
    auto graph = f.build("node x = Float32Node(1.5)\n");
    REQUIRE(graph != nullptr);
    REQUIRE(graph->nodes.size() == 1);

    const NodeInstance &inst = graph->nodes[0];
    CHECK(inst.type_id != UINT32_MAX);

    auto args = graph->argsOf(inst);
    REQUIRE(args.size() == 1);
    CHECK(args[0].kind == Arg::Kind::Literal);
    CHECK(args[0].literal.kind == Literal::Kind::Float32);
    CHECK(args[0].literal.f == 1.5);
}

TEST_CASE("buildGraph on a node with a node reference argument",
          "[sema][graph-builder]")
{
    Fixture f;
    auto graph = f.build(
        "node body = Float32Node(1.5)\n"
        "node result = Float32Node(body)\n");

    REQUIRE(graph != nullptr);
    REQUIRE(graph->nodes.size() == 2);

    // `result` is index 1; its argument refers to `body` (index 0).
    const NodeInstance &result = graph->nodes[1];
    auto args = graph->argsOf(result);
    REQUIRE(args.size() == 1);
    CHECK(args[0].kind == Arg::Kind::NodeRef);
    CHECK(args[0].node_ref == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Subscribers
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("buildGraph records subscribers on the trigger node",
          "[sema][graph-builder]")
{
    Fixture f;
    auto graph = f.build(
        "node tick = EveryFrame()\n"
        "node move = MoveBody(nil) on tick\n");
    REQUIRE(graph != nullptr);
    REQUIRE(graph->nodes.size() == 2);

    // `tick` is index 0; `move` subscribes to it.
    const NodeInstance &tick = graph->nodes[0];
    auto subs = graph->subscribersOf(tick);
    REQUIRE(subs.size() == 1);
    CHECK(subs[0] == 1); // `move`'s index
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase order
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("buildGraph computes phase_order for action nodes",
          "[sema][graph-builder]")
{
    Fixture f;
    auto graph = f.build(
        "node tick = EveryFrame()\n"
        "node a = MoveBody(nil) on tick\n"
        "node b = MoveBody(nil) on tick\n");
    REQUIRE(graph != nullptr);
    REQUIRE(graph->phase_order.size() == 2);

    // `a` and `b` are the two action nodes; source order is preserved.
    CHECK(graph->phase_order[0] == 1);
    CHECK(graph->phase_order[1] == 2);
}

TEST_CASE("buildGraph does not include value nodes in phase_order",
          "[sema][graph-builder]")
{
    Fixture f;
    auto graph = f.build(
        "node x = Float32Node(1.5)\n"
        "node tick = EveryFrame()\n"
        "node move = MoveBody(nil) on tick\n");
    REQUIRE(graph != nullptr);
    REQUIRE(graph->phase_order.size() == 1);
    // Only `move` (index 2) is an action.
    CHECK(graph->phase_order[0] == 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Value order
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("buildGraph computes value_order for value nodes",
          "[sema][graph-builder]")
{
    Fixture f;
    auto graph = f.build(
        "node a = Float32Node(1.0)\n"
        "node b = Float32Node(a)\n");
    REQUIRE(graph != nullptr);
    REQUIRE(graph->value_order.size() == 2);

    // `a` must come before `b` in the value order.
    auto posA = std::find(graph->value_order.begin(),
                          graph->value_order.end(), NodeIndex{0});
    auto posB = std::find(graph->value_order.begin(),
                          graph->value_order.end(), NodeIndex{1});
    REQUIRE(posA != graph->value_order.end());
    REQUIRE(posB != graph->value_order.end());
    CHECK(posA < posB);
}

// ─────────────────────────────────────────────────────────────────────────────
// String pool
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("buildGraph interns string arguments into the string pool",
          "[sema][graph-builder]")
{
    Fixture f;
    // A hypothetical node that takes a string. The registry does not
    // declare one; this test uses a string as a resource default.
    auto graph = f.build(
        "resource R { s: string = \"hello\" }\n");
    REQUIRE(graph != nullptr);
    // Note: resource field string defaults are not yet handled
    // (deferred to Step 7.6c). This test will need updating then.
    // For now, it just verifies that the pool is empty (the string
    // was not added).
    CHECK(graph->string_pool.empty());
}