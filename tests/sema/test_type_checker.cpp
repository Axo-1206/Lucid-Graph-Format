/// @file tests/sema/test_type_checker.cpp
///
/// @brief Tests for Pass 3: type checking.

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

    struct TestRegistry
    {
        // Data that the registry's spans point into.
        std::vector<PhaseInfo> phases;
        std::vector<EnumMemberInfo> keyMembers;
        std::vector<EnumTypeInfo> enums;
        std::vector<HandleTypeInfo> handles;
        std::vector<NodePortInfo> floatArgs;
        std::vector<NodePortInfo> floatOutputs;
        std::vector<NodePortInfo> damageArgs;
        std::vector<NodePortInfo> bodyArgs;
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

            // MoveBody(body: BodyRef)
            bodyArgs.push_back(NodePortInfo{"body",
                                            TypeId::handle("BodyRef")});
            nodeTypes.push_back(NodeTypeInfo{
                "MoveBody", NodeKind::Action, "Physics", 0,
                ArenaSpan<NodePortInfo>(bodyArgs.data(), bodyArgs.size()),
                ArenaSpan<NodePortInfo>{},
                ArenaSpan<NodePortInfo>{}});

            // Damage(body: BodyRef, amount: int32)
            damageArgs.push_back(NodePortInfo{"body",
                                              TypeId::handle("BodyRef")});
            damageArgs.push_back(NodePortInfo{"amount",
                                              TypeId::primitive("int32")});
            nodeTypes.push_back(NodeTypeInfo{
                "Damage", NodeKind::Action, "Physics", 0,
                ArenaSpan<NodePortInfo>(damageArgs.data(), damageArgs.size()),
                ArenaSpan<NodePortInfo>{},
                ArenaSpan<NodePortInfo>{}});

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
        "node m = MoveBody(nil)\n");
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
