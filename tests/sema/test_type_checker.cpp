/// @file tests/sema/test_type_checker.cpp
///
/// @brief Tests for Pass 3: type checking and the trigger rules.

#include "sema/TypeMap.hpp"

#include "core/ast/DeclAST.hpp"
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

    // ─── A small test registry ─────────────────────────────────────────────
    //
    // Declares a handful of node types, enums, handles, and phases that
    // cover the cases the tests exercise.
    //
    // Node types:
    //   Float32Node(value: float32) -> float32     (Value)
    //   MoveBody(body: BodyRef)                    (Action)
    //   Damage(body: BodyRef, amount: int32)       (Action)
    //   EveryFrame()                               (Trigger)
    //   OnCollision(body: BodyRef, tag: string)    (Trigger)

    struct TestRegistry
    {
        // Data that the registry's spans point into.
        std::vector<PhaseInfo> phases;
        std::vector<EnumMemberInfo> keyMembers;
        std::vector<EnumTypeInfo> enums;
        std::vector<HandleTypeInfo> handles;
        std::vector<NodeArgInfo> floatArgs;
        std::vector<NodeArgInfo> moveBodyArgs;
        std::vector<NodeArgInfo> damageArgs;
        std::vector<NodeArgInfo> collisionArgs;
        std::vector<NodeTypeInfo> nodeTypes;
        Registry registry;

        TestRegistry()
        {
            phases.push_back(PhaseInfo{"update"});

            keyMembers.push_back(EnumMemberInfo{"W", 0});
            keyMembers.push_back(EnumMemberInfo{"A", 1});
            enums.push_back(EnumTypeInfo{"Key",
                                         ArenaSpan<EnumMemberInfo>(keyMembers.data(), keyMembers.size())});

            handles.push_back(HandleTypeInfo{"BodyRef"});

            // Float32Node(value: float32) -> float32
            floatArgs.push_back(NodeArgInfo{"value",
                                            TypeId::primitive("float32")});
            nodeTypes.push_back(NodeTypeInfo{
                "Float32Node", NodeKind::Value, "Math", 0,
                ArenaSpan<NodeArgInfo>(floatArgs.data(), floatArgs.size()),
                TypeId::primitive("float32")});

            // MoveBody(body: BodyRef)
            moveBodyArgs.push_back(NodeArgInfo{"body",
                                               TypeId::handle("BodyRef")});
            nodeTypes.push_back(NodeTypeInfo{
                "MoveBody", NodeKind::Action, "Physics", 0,
                ArenaSpan<NodeArgInfo>(moveBodyArgs.data(),
                                       moveBodyArgs.size()),
                TypeId{}});

            // Damage(body: BodyRef, amount: int32)
            damageArgs.push_back(NodeArgInfo{"body",
                                             TypeId::handle("BodyRef")});
            damageArgs.push_back(NodeArgInfo{"amount",
                                             TypeId::primitive("int32")});
            nodeTypes.push_back(NodeTypeInfo{
                "Damage", NodeKind::Action, "Physics", 0,
                ArenaSpan<NodeArgInfo>(damageArgs.data(), damageArgs.size()),
                TypeId{}});

            // EveryFrame()
            nodeTypes.push_back(NodeTypeInfo{
                "EveryFrame", NodeKind::Trigger, "Flow", 0,
                ArenaSpan<NodeArgInfo>{},
                TypeId{}});

            // OnCollision(body: BodyRef, tag: string)
            collisionArgs.push_back(NodeArgInfo{"body",
                                                TypeId::handle("BodyRef")});
            collisionArgs.push_back(NodeArgInfo{"tag",
                                                TypeId::primitive("string")});
            nodeTypes.push_back(NodeTypeInfo{
                "OnCollision", NodeKind::Trigger, "Physics", 0,
                ArenaSpan<NodeArgInfo>(collisionArgs.data(),
                                       collisionArgs.size()),
                TypeId{}});

            registry.phases = ArenaSpan<PhaseInfo>(phases.data(),
                                                   phases.size());
            registry.enums = ArenaSpan<EnumTypeInfo>(enums.data(),
                                                     enums.size());
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

        struct Run
        {
            ModuleAST *module;
            TypeMap types;
        };

        Run run(std::string_view source)
        {
            ParserContext ctx(pool, arena, diag, stream);
            ModuleAST *module = parseFile("test.lucid", source, ctx);

            SymbolTable symbols;
            collectSymbols(module, symbols, diag);

            ResolutionMap resolutions;
            resolveNames(module, symbols, resolutions, diag);

            TypeMap types;
            checkTypes(module, symbols, resolutions, reg.registry, types, diag);

            Run r;
            r.module = module;
            r.types = std::move(types);
            return r;
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// TypeId resolution
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("type checker resolves a primitive type",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("resource R { x: float32 }\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("type checker resolves a primitive alias",
          "[sema][type-checker]")
{
    Fixture f;
    // `float` should normalize to `float32`.
    auto r = f.run("resource R { x: float }\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("type checker resolves a handle type",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("resource R { b: BodyRef }\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("type checker reports an unknown type",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("resource R { x: Unknown }\n");
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Type_UnknownType);
}

// ─────────────────────────────────────────────────────────────────────────────
// Node expressions
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("type checker accepts a valid node expression",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("node speed = Float32Node(1.5)\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("type checker reports an unknown node type",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("node x = UnknownNode()\n");
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Type_UnknownNodeType);
}

TEST_CASE("type checker reports an arg-count mismatch",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("node x = Float32Node(1.0, 2.0)\n");
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Type_ArgCountMismatch);
}

TEST_CASE("type checker reports an arg-type mismatch",
          "[sema][type-checker]")
{
    Fixture f;
    // Float32Node expects a float32; passing a string is a mismatch.
    auto r = f.run("node x = Float32Node(\"hi\")\n");
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Type_Mismatch);
}

TEST_CASE("type checker accepts nil for a handle port",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run(
        "resource Player { }\n"
        "node tick = EveryFrame()\n"
        "node m = MoveBody(nil) on tick\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("type checker rejects nil for a non-handle port",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("node x = Float32Node(nil)\n");
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Type_Mismatch);
}

// ─────────────────────────────────────────────────────────────────────────────
// Resource defaults
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("type checker accepts a matching default",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("resource R { x: float32 = 1.5 }\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("type checker accepts an int default for an int field",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("resource R { x: int32 = 42 }\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("type checker rejects a mismatched default",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("resource R { x: int32 = 1.5 }\n");
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Type_InvalidDefault);
}

TEST_CASE("type checker accepts nil for a handle field default",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("resource R { b: BodyRef = nil }\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("type checker rejects nil for a non-handle default",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("resource R { x: int32 = nil }\n");
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Type_InvalidDefault);
}

TEST_CASE("type checker accepts an enum member default",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run(
        "enum Direction { N, S, E, W }\n"
        "resource Config { dir: Direction = Direction.N }\n");
    CHECK_FALSE(f.diag.hasErrors());
}

// ─────────────────────────────────────────────────────────────────────────────
// Trigger rules
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("type checker accepts an action node with an `on` clause",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run(
        "node on_hit = OnCollision(nil, \"hazard\")\n"
        "node m = MoveBody(nil) on on_hit\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("type checker rejects an action node with no `on` clause",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run("node m = MoveBody(nil)\n");
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code == DiagCode::Trigger_ActionWithoutOn);
}

TEST_CASE("type checker accepts a trigger node as an `on` target",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run(
        "node tick = EveryFrame()\n"
        "node m = MoveBody(nil) on tick\n");
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("type checker rejects a value node as an `on` target",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run(
        "node speed = Float32Node(1.5)\n"
        "node m = MoveBody(nil) on speed\n");
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code ==
          DiagCode::Trigger_OnTargetNotTrigger);
}

TEST_CASE("type checker rejects an action node as an `on` target",
          "[sema][type-checker]")
{
    Fixture f;
    auto r = f.run(
        "node other = MoveBody(nil) on other\n"
        "node m = MoveBody(nil) on other\n");
    // `other` is itself an action node (with a self-trigger, which is
    // also an error). The second `on other` targets a non-trigger.
    CHECK(f.diag.hasErrors());
}