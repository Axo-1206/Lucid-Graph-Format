/// @file tests/core/test_module_ast.cpp
///
/// @brief Tests for ModuleAST.

#include "core/ast/ModuleAST.hpp"
#include "core/ast/DeclAST.hpp"
#include "core/ast/AttributeAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/ast/ValueAST.hpp"
#include "core/ast/BaseAST.hpp"
#include "core/Tokens.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ModuleAST: default has the Module kind",
          "[core][ast][module]")
{
    ModuleAST m;
    REQUIRE(m.kind == ASTKind::Module);
    REQUIRE(m.isa<ModuleAST>());
}

TEST_CASE("ModuleAST: default fields are empty",
          "[core][ast][module]")
{
    ModuleAST m;
    REQUIRE_FALSE(m.filePath.isValid());
    REQUIRE(m.decls.empty());
    REQUIRE_FALSE(m.hasErrors);
    REQUIRE(m.isEmpty());
    REQUIRE(m.declCount() == 0);
}

TEST_CASE("ModuleAST: filePath is stored",
          "[core][ast][module]")
{
    StringPool pool;
    ModuleAST m{pool.intern("player.lucid"), {}};

    REQUIRE(pool.lookupView(m.filePath) == "player.lucid");
}

// ─────────────────────────────────────────────────────────────────────────────
// decls span
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ModuleAST: decls span holds the declarations in order",
          "[core][ast][module]")
{
    ASTArena arena;
    StringPool pool;

    auto *en = arena.make<EnumDeclAST>(pool.intern("Color"),
                                       ArenaSpan<EnumMemberAST *>{});
    auto *res = arena.make<ResourceDeclAST>(pool.intern("Config"),
                                            ArenaSpan<ResourceFieldAST *>{});
    auto *nd = arena.make<NodeDeclAST>(pool.intern("speed"), nullptr);

    auto decls = arena.makeSpan<DeclAST *>({
        static_cast<DeclAST *>(en),
        static_cast<DeclAST *>(res),
        static_cast<DeclAST *>(nd),
    });

    ModuleAST m{pool.intern("test.lucid"), decls};

    REQUIRE_FALSE(m.isEmpty());
    REQUIRE(m.declCount() == 3);
    REQUIRE(m.decls[0]->isa<EnumDeclAST>());
    REQUIRE(m.decls[1]->isa<ResourceDeclAST>());
    REQUIRE(m.decls[2]->isa<NodeDeclAST>());
}

// ─────────────────────────────────────────────────────────────────────────────
// One of every top-level declaration kind
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ModuleAST: a module with one of every top-level declaration kind",
          "[core][ast][module][integration]")
{
    // A representative module:
    //
    //     import core.keys as k
    //     enum Color { Red, Green, Blue }
    //     resource Config { speed: float = 200.0 }
    //     node speed = Float32Node(200.0)
    ASTArena arena;
    StringPool pool;

    // import core.keys as k
    auto *imp = arena.make<ImportDeclAST>(
        pool.intern("core.keys"), pool.intern("k"));

    // enum Color { Red, Green, Blue }
    auto *memberRed = arena.make<EnumMemberAST>(pool.intern("Red"));
    auto *memberGreen = arena.make<EnumMemberAST>(pool.intern("Green"));
    auto *memberBlue = arena.make<EnumMemberAST>(pool.intern("Blue"));
    auto members = arena.makeSpan<EnumMemberAST *>(
        {memberRed, memberGreen, memberBlue});
    auto *en = arena.make<EnumDeclAST>(pool.intern("Color"), members);

    // resource Config { speed: float = 200.0 }
    auto *floatType = arena.make<TypeIdAST>(pool.intern("float"));
    auto *speedLit = arena.make<LiteralValueAST>(
        LiteralKind::Float, pool.intern("200.0"));
    auto *speedField = arena.make<ResourceFieldAST>(
        pool.intern("speed"), floatType, speedLit);
    auto fields = arena.makeSpan<ResourceFieldAST *>({speedField});
    auto *res = arena.make<ResourceDeclAST>(pool.intern("Config"), fields);

    // node speed = Float32Node(200.0)
    auto *nodeType = arena.make<TypeIdAST>(pool.intern("Float32Node"));
    auto *nodeArg = arena.make<LiteralValueAST>(
        LiteralKind::Float, pool.intern("200.0"));
    auto nodeArgs = arena.makeSpan<BaseAST *>({static_cast<BaseAST *>(nodeArg)});
    auto *nodeExpr = arena.make<NodeExprAST>(nodeType, nodeArgs);
    auto *node = arena.make<NodeDeclAST>(pool.intern("speed"), nodeExpr);

    auto decls = arena.makeSpan<DeclAST *>({
        static_cast<DeclAST *>(imp),
        static_cast<DeclAST *>(en),
        static_cast<DeclAST *>(res),
        static_cast<DeclAST *>(node),
    });

    ModuleAST m{pool.intern("test.lucid"), decls};

    REQUIRE(m.declCount() == 4);
    REQUIRE(m.decls[0]->isa<ImportDeclAST>());
    REQUIRE(m.decls[1]->isa<EnumDeclAST>());
    REQUIRE(m.decls[2]->isa<ResourceDeclAST>());
    REQUIRE(m.decls[3]->isa<NodeDeclAST>());

    // Spot-check a leaf field through the base pointer.
    REQUIRE(pool.lookupView(m.decls[0]->as<ImportDeclAST>()->path) == "core.keys");
    REQUIRE(m.decls[1]->as<EnumDeclAST>()->members.size() == 3);
    REQUIRE(m.decls[2]->as<ResourceDeclAST>()->fields.size() == 1);
    REQUIRE(m.decls[3]->as<NodeDeclAST>()->expr->args.size() == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// hasErrors
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ModuleAST: hasErrors defaults to false",
          "[core][ast][module]")
{
    ModuleAST m;
    REQUIRE_FALSE(m.hasErrors);
}

TEST_CASE("ModuleAST: hasErrors can be set",
          "[core][ast][module]")
{
    ModuleAST m;
    m.hasErrors = true;
    REQUIRE(m.hasErrors);
}

// ─────────────────────────────────────────────────────────────────────────────
// isa / as
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ModuleAST: isa returns true for ModuleAST",
          "[core][ast][module]")
{
    ModuleAST m;
    REQUIRE(m.isa<ModuleAST>());
}

TEST_CASE("ModuleAST: isa returns false for other kinds",
          "[core][ast][module]")
{
    ModuleAST m;
    REQUIRE_FALSE(m.isa<DeclAST>());
    REQUIRE_FALSE(m.isa<ImportDeclAST>());
    REQUIRE_FALSE(m.isa<UnknownAST>());
}

TEST_CASE("ModuleAST: as through a BaseAST pointer works",
          "[core][ast][module]")
{
    StringPool pool;
    ModuleAST concrete{pool.intern("test.lucid"), {}};

    BaseAST *base = &concrete;
    REQUIRE(base->isa<ModuleAST>());
    REQUIRE(base->as<ModuleAST>()->filePath == pool.intern("test.lucid"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Empty module
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ModuleAST: an empty file is a valid module",
          "[core][ast][module]")
{
    StringPool pool;
    ModuleAST m{pool.intern("empty.lucid"), {}};

    REQUIRE(m.isEmpty());
    REQUIRE(m.declCount() == 0);
    REQUIRE_FALSE(m.hasErrors);
}