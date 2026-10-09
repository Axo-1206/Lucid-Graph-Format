/// @file tests/sema/test_dead_code.cpp
///
/// @brief Tests for Pass 5: dead-code detection.

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

#include "sema/DeadCodeChecker.hpp"
#include "sema/Resolver.hpp"
#include "sema/SymbolCollector.hpp"
#include "sema/TypeChecker.hpp"

using lucid::diag::DiagnosticEngine;
using lucid::parser::parseFile;
using lucid::parser::ParserContext;
using lucid::parser::TokenStream;
using namespace lucid::sema;
using namespace lucid::diag;

namespace
{

    struct TestRegistry
    {
        std::vector<PhaseInfo> phases;
        std::vector<HandleTypeInfo> handles;
        std::vector<NodeArgInfo> floatArgs;
        std::vector<NodeArgInfo> actionArgs;
        std::vector<NodeArgInfo> printArgs;
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

            // PrintNode(value: float32)  -- Action
            printArgs.push_back(NodeArgInfo{"value",
                                            TypeId::primitive("float32")});
            nodeTypes.push_back(NodeTypeInfo{
                "PrintNode", NodeKind::Action, "Debug", 0,
                ArenaSpan<NodeArgInfo>(printArgs.data(), printArgs.size()),
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

        /// Returns the number of Warn_DeadNode warnings after running
        /// the full pipeline (parse, resolve, type-check, check dead code).
        int countDeadNodeWarnings(std::string_view source)
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
                return -1; // compile failed; test should not run this path
            }

            checkDeadCode(module, resolutions, reg.registry, diag);

            int count = 0;
            for (const auto &d : diag.all())
            {
                if (d.code == DiagCode::Warn_DeadNode)
                    ++count;
            }
            return count;
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Used value nodes
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dead-code check reports no warning for a used value node",
          "[sema][dead-code]")
{
    Fixture f;
    // `a` is used by `print`; `print` is an action, not checked.
    // `tick` is a trigger, not checked. Zero warnings.
    const int warnings = f.countDeadNodeWarnings(
        "node a = Float32Node(1.0)\n"
        "node tick = EveryFrame()\n"
        "node print = PrintNode(a) on tick\n");
    CHECK(warnings == 0);
}

TEST_CASE("dead-code check reports no warning for a value used by an action",
          "[sema][dead-code]")
{
    Fixture f;
    // A two-step chain ending at an action. `a` is used by `b`,
    // `b` is used by `print`. Zero warnings.
    const int warnings = f.countDeadNodeWarnings(
        "node a = Float32Node(1.0)\n"
        "node b = Float32Node(a)\n"
        "node tick = EveryFrame()\n"
        "node print = PrintNode(b) on tick\n");
    CHECK(warnings == 0);
}

TEST_CASE("dead-code check reports the leaf of an unfinished chain",
          "[sema][dead-code]")
{
    Fixture f;
    // `a` is used by `b`; `b` is not used by anything. Only `b` is
    // reported. This is the "simple rule": a node is dead if no other
    // node references it. `a` is considered used because `b`
    // references it, even though `b` is itself dead.
    const int warnings = f.countDeadNodeWarnings(
        "node a = Float32Node(1.0)\n"
        "node b = Float32Node(a)\n");
    CHECK(warnings == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Unused value nodes
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dead-code check reports an unused value node",
          "[sema][dead-code]")
{
    Fixture f;
    const int warnings = f.countDeadNodeWarnings(
        "node unused = Float32Node(1.0)\n");
    CHECK(warnings == 1);
}

TEST_CASE("dead-code check reports multiple unused value nodes",
          "[sema][dead-code]")
{
    Fixture f;
    const int warnings = f.countDeadNodeWarnings(
        "node a = Float32Node(1.0)\n"
        "node b = Float32Node(2.0)\n"
        "node c = Float32Node(3.0)\n");
    CHECK(warnings == 3);
}

TEST_CASE("dead-code check reports the last node in a chain",
          "[sema][dead-code]")
{
    Fixture f;
    // `a` is used by `b`, `b` is used by `c`, `c` is not used.
    const int warnings = f.countDeadNodeWarnings(
        "node a = Float32Node(1.0)\n"
        "node b = Float32Node(a)\n"
        "node c = Float32Node(b)\n");
    CHECK(warnings == 1);
}

TEST_CASE("dead-code check does not report action nodes",
          "[sema][dead-code]")
{
    Fixture f;
    // An action node with no `on` clause is already an error (caught
    // by the type checker). Here we use an action node with an `on`
    // clause; it is not a value node and is not checked.
    const int warnings = f.countDeadNodeWarnings(
        "node tick = EveryFrame()\n"
        "node move = MoveBody(nil) on tick\n");
    CHECK(warnings == 0);
}

TEST_CASE("dead-code check does not report trigger nodes",
          "[sema][dead-code]")
{
    Fixture f;
    // A trigger node with no subscribers is not reported. (Whether
    // a subscriber-less trigger is dead is a separate question;
    // the current check reports only value nodes.)
    const int warnings = f.countDeadNodeWarnings(
        "node tick = EveryFrame()\n");
    CHECK(warnings == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Dead-code warnings do not fail the compile
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("dead-code warning does not set hasErrors",
          "[sema][dead-code]")
{
    Fixture f;
    const int warnings = f.countDeadNodeWarnings(
        "node unused = Float32Node(1.0)\n");
    CHECK(warnings >= 1);
    // Warnings only; hasErrors is false.
    CHECK_FALSE(f.diag.hasErrors());
    CHECK(f.diag.hasWarnings());
}