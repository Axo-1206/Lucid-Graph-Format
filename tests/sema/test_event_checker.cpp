/// @file tests/sema/test_event_checker.cpp
///
/// @brief Tests for Pass 4: the Event rules.

#include "sema/ResolutionMap.hpp"
#include "sema/SymbolTable.hpp"

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

#include "sema/EventChecker.hpp"
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

    // ─── A small registry with trigger and action nodes ─────────────────────

    struct TestRegistry
    {
        std::vector<PhaseInfo> phases;
        std::vector<HandleTypeInfo> handles;
        std::vector<NodePortInfo> bodyArgs;
        std::vector<NodePortInfo> floatOutputs;
        std::vector<NodePortInfo> floatArgs;
        std::vector<NodeTypeInfo> nodeTypes;
        Registry registry;

        TestRegistry()
        {
            phases.push_back(PhaseInfo{"update"});
            handles.push_back(HandleTypeInfo{"BodyRef"});

            // MoveBody(body: BodyRef)   -- Action
            bodyArgs.push_back(NodePortInfo{"body",
                                            TypeId::handle("BodyRef")});
            nodeTypes.push_back(NodeTypeInfo{
                "MoveBody", NodeKind::Action, "Physics", 0,
                ArenaSpan<NodePortInfo>(bodyArgs.data(), bodyArgs.size()),
                ArenaSpan<NodePortInfo>{},
                ArenaSpan<NodePortInfo>{}});

            // Float32Node(value: float32) -> float32   -- Value
            floatArgs.push_back(NodePortInfo{"value",
                                             TypeId::primitive("float32")});
            floatOutputs.push_back(NodePortInfo{"out",
                                                TypeId::primitive("float32")});
            nodeTypes.push_back(NodeTypeInfo{
                "Float32Node", NodeKind::Value, "Math", 0,
                ArenaSpan<NodePortInfo>(floatArgs.data(), floatArgs.size()),
                ArenaSpan<NodePortInfo>(floatOutputs.data(),
                                        floatOutputs.size()),
                ArenaSpan<NodePortInfo>{}});

            // EveryFrame()   -- Trigger
            nodeTypes.push_back(NodeTypeInfo{
                "EveryFrame", NodeKind::Trigger, "Flow", 0,
                ArenaSpan<NodePortInfo>{},
                ArenaSpan<NodePortInfo>{},
                ArenaSpan<NodePortInfo>{}});

            // When(cond: bool)   -- Trigger
            // (Skip for tests; only EveryFrame is used as a trigger target.)

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

        void run(std::string_view source)
        {
            ParserContext ctx(pool, arena, diag, stream);
            ModuleAST *module = parseFile("test.lucid", source, ctx);

            SymbolTable symbols;
            collectSymbols(module, symbols, diag);

            ResolutionMap resolutions;
            resolveNames(module, symbols, resolutions, diag);

            TypeMap types;
            checkTypes(module, symbols, resolutions, reg.registry, types, diag);

            checkEvents(module, symbols, resolutions, reg.registry, diag);
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Rule 2: `on` targets must be trigger sources
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("EventChecker accepts a trigger target",
          "[sema][event-checker]")
{
    Fixture f;
    f.run(
        "node on_frame = EveryFrame()\n"
        "node move = MoveBody(nil) on on_frame\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("EventChecker rejects a value node as an on target",
          "[sema][event-checker]")
{
    Fixture f;
    f.run(
        "node speed = Float32Node(1.0)\n"
        "node move = MoveBody(nil) on speed\n");
    CHECK(f.diag.hasErrors());

    bool found = false;
    for (const auto &d : f.diag.all())
    {
        if (d.code == lucid::diag::DiagCode::Event_OnTargetNotTrigger)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("EventChecker rejects an action node as an on target",
          "[sema][event-checker]")
{
    Fixture f;
    // `mover` is an Action, not a Trigger. It cannot be an `on` target.
    f.run(
        "node mover = MoveBody(nil) on every\n"
        "node every = EveryFrame()\n"
        "node move2 = MoveBody(nil) on mover\n");

    bool found = false;
    for (const auto &d : f.diag.all())
    {
        if (d.code == lucid::diag::DiagCode::Event_OnTargetNotTrigger)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 3: action nodes need `on`
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("EventChecker accepts an action node with an on clause",
          "[sema][event-checker]")
{
    Fixture f;
    f.run(
        "node every = EveryFrame()\n"
        "node move = MoveBody(nil) on every\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("EventChecker rejects an action node without on",
          "[sema][event-checker]")
{
    Fixture f;
    f.run("node move = MoveBody(nil)\n");

    bool found = false;
    for (const auto &d : f.diag.all())
    {
        if (d.code == lucid::diag::DiagCode::Event_ActionWithoutOn)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("EventChecker does not require on for a value node",
          "[sema][event-checker]")
{
    Fixture f;
    f.run("node speed = Float32Node(1.0)\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("EventChecker does not require on for a trigger node",
          "[sema][event-checker]")
{
    Fixture f;
    f.run("node on_frame = EveryFrame()\n");
    CHECK_FALSE(f.diag.hasErrors());
}

// ─────────────────────────────────────────────────────────────────────────────
// Rule 1: Event as a node port type (defensive)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("EventChecker accepts a registry without Event ports",
          "[sema][event-checker]")
{
    Fixture f;
    f.run("");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("EventChecker reports a registry with an Event port",
          "[sema][event-checker]")
{
    // Build a registry with a port that has type Event.
    std::vector<NodePortInfo> badArgs;
    badArgs.push_back(NodePortInfo{"evt", TypeId::event()});
    std::vector<NodeTypeInfo> nodeTypes;
    nodeTypes.push_back(NodeTypeInfo{
        "BadNode", NodeKind::Action, "", 0,
        ArenaSpan<NodePortInfo>(badArgs.data(), badArgs.size()),
        ArenaSpan<NodePortInfo>{},
        ArenaSpan<NodePortInfo>{}});
    Registry reg;
    reg.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes.data(),
                                            nodeTypes.size());

    StringPool pool;
    ASTArena arena;
    DiagnosticEngine diag(&pool);
    TokenStream stream(std::vector<Token>{
        Token{TokenType::EOF_TOKEN, InternedString{}, SourceLocation{1, 1}}});
    ParserContext ctx(pool, arena, diag, stream);
    ModuleAST *module = parseFile("test.lucid", "", ctx);

    SymbolTable symbols;
    collectSymbols(module, symbols, diag);

    ResolutionMap resolutions;
    resolveNames(module, symbols, resolutions, diag);

    TypeMap types;
    checkTypes(module, symbols, resolutions, reg, types, diag);

    checkEvents(module, symbols, resolutions, reg, diag);

    bool found = false;
    for (const auto &d : diag.all())
    {
        if (d.code == lucid::diag::DiagCode::Event_PortNotAllowed)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}