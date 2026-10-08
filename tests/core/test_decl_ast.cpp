/// @file tests/core/test_decl_ast.cpp
///
/// @brief Tests for DeclAST.hpp: the family base and the eight concrete
///        declaration nodes.

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
// DeclAST family base
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("DeclAST: family base has the Decl kind",
          "[core][ast][decl]")
{
    // DeclAST is not meant to be constructed directly in real code, but
    // the base's fields are what every concrete declaration shares.
    // A minimal concrete subclass exercises them without a real
    // declaration node.
    struct LocalDecl : DeclAST
    {
        LocalDecl() : DeclAST(ASTKind::Decl, InternedString{}) {}
    };

    LocalDecl d;
    REQUIRE(d.kind == ASTKind::Decl);
    REQUIRE(d.isa<DeclAST>());
    REQUIRE(d.attributes.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// ImportDeclAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ImportDeclAST: default", "[core][ast][decl][import]")
{
    ImportDeclAST d;
    REQUIRE(d.kind == ASTKind::ImportDecl);
    REQUIRE(d.isa<ImportDeclAST>());
    REQUIRE(d.isa<DeclAST>());
    REQUIRE_FALSE(d.path.isValid());
    REQUIRE_FALSE(d.name.isValid());
}

TEST_CASE("ImportDeclAST: `import core.keys`",
          "[core][ast][decl][import]")
{
    StringPool pool;
    ImportDeclAST d{pool.intern("core.keys"), pool.intern("keys")};

    REQUIRE(pool.lookupView(d.path) == "core.keys");
    REQUIRE(pool.lookupView(d.name) == "keys");
    REQUIRE(d.attributes.empty());
}

TEST_CASE("ImportDeclAST: `import health` (a single-segment path)",
          "[core][ast][decl][import]")
{
    StringPool pool;
    ImportDeclAST d{pool.intern("health"), pool.intern("health")};

    REQUIRE(pool.lookupView(d.path) == "health");
    REQUIRE(pool.lookupView(d.name) == "health");
}

TEST_CASE("ImportDeclAST: isa distinguishes it from other declarations",
          "[core][ast][decl][import]")
{
    ImportDeclAST d;
    REQUIRE_FALSE(d.isa<EnumDeclAST>());
    REQUIRE_FALSE(d.isa<ResourceDeclAST>());
    REQUIRE_FALSE(d.isa<NodeDeclAST>());
}

// ─────────────────────────────────────────────────────────────────────────────
// EnumDeclAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("EnumDeclAST: default", "[core][ast][decl][enum]")
{
    EnumDeclAST d;
    REQUIRE(d.kind == ASTKind::EnumDecl);
    REQUIRE(d.isa<EnumDeclAST>());
    REQUIRE(d.isa<DeclAST>());
    REQUIRE(d.members.empty());
}

TEST_CASE("EnumDeclAST: `enum Key { A, B, C }`",
          "[core][ast][decl][enum]")
{
    ASTArena arena;
    StringPool pool;

    auto *memberA = arena.make<EnumMemberAST>(pool.intern("A"));
    auto *memberB = arena.make<EnumMemberAST>(pool.intern("B"));
    auto *memberC = arena.make<EnumMemberAST>(pool.intern("C"));
    auto members =
        arena.makeSpan<EnumMemberAST *>({memberA, memberB, memberC});

    EnumDeclAST d{pool.intern("Key"), members};

    REQUIRE(pool.lookupView(d.name) == "Key");
    REQUIRE(d.members.size() == 3);
    REQUIRE(pool.lookupView(d.members[0]->name) == "A");
    REQUIRE(pool.lookupView(d.members[1]->name) == "B");
    REQUIRE(pool.lookupView(d.members[2]->name) == "C");
}

TEST_CASE("EnumDeclAST: empty enum",
          "[core][ast][decl][enum]")
{
    StringPool pool;
    EnumDeclAST d{pool.intern("Empty"), {}};

    REQUIRE(pool.lookupView(d.name) == "Empty");
    REQUIRE(d.members.empty());
}

TEST_CASE("EnumDeclAST: isa distinguishes it from other declarations",
          "[core][ast][decl][enum]")
{
    EnumDeclAST d;
    REQUIRE_FALSE(d.isa<ImportDeclAST>());
    REQUIRE_FALSE(d.isa<ResourceDeclAST>());
    REQUIRE_FALSE(d.isa<NodeDeclAST>());
}

// ─────────────────────────────────────────────────────────────────────────────
// ResourceFieldAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ResourceFieldAST: default", "[core][ast][decl][field]")
{
    ResourceFieldAST f;
    REQUIRE(f.kind == ASTKind::ResourceField);
    REQUIRE(f.isa<ResourceFieldAST>());
    REQUIRE(f.type == nullptr);
    REQUIRE(f.defaultValue == nullptr);
    REQUIRE_FALSE(f.hasDefault());
}

TEST_CASE("ResourceFieldAST: `speed: float`",
          "[core][ast][decl][field]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("float"));
    ResourceFieldAST f{pool.intern("speed"), type};

    REQUIRE(pool.lookupView(f.name) == "speed");
    REQUIRE(f.type == type);
    REQUIRE_FALSE(f.hasDefault());
    REQUIRE(f.defaultValue == nullptr);
}

TEST_CASE("ResourceFieldAST: `speed: float = 200.0`",
          "[core][ast][decl][field]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("float"));
    auto *def = arena.make<LiteralValueAST>(
        LiteralKind::Float, pool.intern("200.0"));

    ResourceFieldAST f{pool.intern("speed"), type, def};

    REQUIRE(pool.lookupView(f.name) == "speed");
    REQUIRE(f.type == type);
    REQUIRE(f.hasDefault());
    REQUIRE(f.defaultValue == def);

    // defaultValue is a BaseAST*. The node kind is LiteralValue; the
    // literal's kind is a separate field inside LiteralValueAST.
    REQUIRE(f.defaultValue->isa<LiteralValueAST>());
    REQUIRE(f.defaultValue->as<LiteralValueAST>()->kind == LiteralKind::Float);
}

TEST_CASE("ResourceFieldAST: a field default with a string literal",
          "[core][ast][decl][field]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("string"));
    auto *def = arena.make<LiteralValueAST>(
        LiteralKind::String, pool.intern("hello"));

    ResourceFieldAST f{pool.intern("greeting"), type, def};

    REQUIRE(f.hasDefault());
    REQUIRE(f.defaultValue->isa<LiteralValueAST>());
    REQUIRE(f.defaultValue->as<LiteralValueAST>()->kind == LiteralKind::String);
}

TEST_CASE("ResourceFieldAST: a field default with an enum member access",
          "[core][ast][decl][field]")
{
    // `key_left: Key = Key.A`
    //
    // The default is a value, not a literal. The field access is the
    // reason ResourceFieldAST's constructor takes BaseAST* and not
    // LiteralValueAST*.
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("Key"));
    auto *def = arena.make<FieldAccessValueAST>(
        pool.intern("Key"), pool.intern("A"));

    ResourceFieldAST f{pool.intern("key_left"), type, def};

    REQUIRE(f.hasDefault());
    REQUIRE(f.defaultValue == def);
    REQUIRE(f.defaultValue->isa<FieldAccessValueAST>());
    REQUIRE(f.defaultValue->as<FieldAccessValueAST>()->object ==
            pool.intern("Key"));
    REQUIRE(f.defaultValue->as<FieldAccessValueAST>()->field ==
            pool.intern("A"));
}

TEST_CASE("ResourceFieldAST: a field default with a bare identifier",
          "[core][ast][decl][field]")
{
    // `body: BodyRef = player_body`
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("BodyRef"));
    auto *def = arena.make<IdentifierValueAST>(pool.intern("player_body"));

    ResourceFieldAST f{pool.intern("body"), type, def};

    REQUIRE(f.hasDefault());
    REQUIRE(f.defaultValue->isa<IdentifierValueAST>());
    REQUIRE(f.defaultValue->as<IdentifierValueAST>()->name ==
            pool.intern("player_body"));
}

TEST_CASE("ResourceFieldAST: a field default with an inline node",
          "[core][ast][decl][field]")
{
    // `speed: float = Float32Node(1.0)`
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("float"));
    auto *arg = arena.make<LiteralValueAST>(
        LiteralKind::Float, pool.intern("1.0"));
    auto args = arena.makeSpan<BaseAST *>({static_cast<BaseAST *>(arg)});
    auto *nodeType = arena.make<TypeIdAST>(pool.intern("Float32Node"));
    auto *expr = arena.make<NodeExprAST>(nodeType, args);
    auto *def = arena.make<InlineNodeValueAST>(expr);

    ResourceFieldAST f{pool.intern("speed"), type, def};

    REQUIRE(f.hasDefault());
    REQUIRE(f.defaultValue->isa<InlineNodeValueAST>());
    REQUIRE(f.defaultValue->as<InlineNodeValueAST>()->node == expr);
}

TEST_CASE("ResourceFieldAST: no default vs. explicit zero default",
          "[core][ast][decl][field]")
{
    // `hasDefault()` distinguishes "no default written" from "default
    // written as the zero value of the type." Both are valid; Sema treats
    // them differently when it initializes the field.
    ASTArena arena;
    StringPool pool;

    auto *intType = arena.make<TypeIdAST>(pool.intern("int"));

    // No default: hasDefault() is false.
    ResourceFieldAST noDefault{pool.intern("hp"), intType};
    REQUIRE_FALSE(noDefault.hasDefault());
    REQUIRE(noDefault.defaultValue == nullptr);

    // Explicit zero: hasDefault() is true.
    auto *zero = arena.make<LiteralValueAST>(
        LiteralKind::Int, pool.intern("0"));
    ResourceFieldAST explicitZero{pool.intern("hp"), intType, zero};
    REQUIRE(explicitZero.hasDefault());
    REQUIRE(explicitZero.defaultValue == zero);
}

// ─────────────────────────────────────────────────────────────────────────────
// ResourceDeclAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ResourceDeclAST: default", "[core][ast][decl][resource]")
{
    ResourceDeclAST d;
    REQUIRE(d.kind == ASTKind::ResourceDecl);
    REQUIRE(d.isa<ResourceDeclAST>());
    REQUIRE(d.isa<DeclAST>());
    REQUIRE(d.fields.empty());
}

TEST_CASE("ResourceDeclAST: `resource PlayerState { hp: int, dead: bool }`",
          "[core][ast][decl][resource]")
{
    ASTArena arena;
    StringPool pool;

    auto *intType = arena.make<TypeIdAST>(pool.intern("int"));
    auto *boolType = arena.make<TypeIdAST>(pool.intern("bool"));

    auto *hp = arena.make<ResourceFieldAST>(pool.intern("hp"), intType);
    auto *dead = arena.make<ResourceFieldAST>(pool.intern("dead"), boolType);

    auto fields = arena.makeSpan<ResourceFieldAST *>({hp, dead});

    ResourceDeclAST d{pool.intern("PlayerState"), fields};

    REQUIRE(pool.lookupView(d.name) == "PlayerState");
    REQUIRE(d.fields.size() == 2);
    REQUIRE(pool.lookupView(d.fields[0]->name) == "hp");
    REQUIRE(pool.lookupView(d.fields[1]->name) == "dead");
}

TEST_CASE("ResourceDeclAST: with attributes",
          "[core][ast][decl][resource]")
{
    ASTArena arena;
    StringPool pool;

    auto *exportAttr = arena.make<AttributeAST>(pool.intern("export"));
    auto attrs = arena.makeSpan<AttributeAST *>({exportAttr});

    ResourceDeclAST d{pool.intern("PlayerConfig"), {}, attrs};

    REQUIRE(pool.lookupView(d.name) == "PlayerConfig");
    REQUIRE(d.attributes.size() == 1);
    REQUIRE(d.attributes[0] == exportAttr);
    REQUIRE(pool.lookupView(d.attributes[0]->name) == "export");
}

TEST_CASE("ResourceDeclAST: isa distinguishes it from other declarations",
          "[core][ast][decl][resource]")
{
    ResourceDeclAST d;
    REQUIRE_FALSE(d.isa<ImportDeclAST>());
    REQUIRE_FALSE(d.isa<EnumDeclAST>());
    REQUIRE_FALSE(d.isa<NodeDeclAST>());
}

// ─────────────────────────────────────────────────────────────────────────────
// NodeDeclAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("NodeDeclAST: default", "[core][ast][decl][node]")
{
    NodeDeclAST d;
    REQUIRE(d.kind == ASTKind::NodeDecl);
    REQUIRE(d.isa<NodeDeclAST>());
    REQUIRE(d.isa<DeclAST>());
    REQUIRE(d.expr == nullptr);
    REQUIRE(d.triggers.empty());
    REQUIRE_FALSE(d.hasTriggers());
}

TEST_CASE("NodeDeclAST: `node speed = Float32Node(200.0)`",
          "[core][ast][decl][node]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("Float32Node"));
    auto *arg = arena.make<LiteralValueAST>(
        LiteralKind::Float, pool.intern("200.0"));
    auto args = arena.makeSpan<BaseAST *>({static_cast<BaseAST *>(arg)});
    auto *expr = arena.make<NodeExprAST>(type, args);

    NodeDeclAST d{pool.intern("speed"), expr};

    REQUIRE(pool.lookupView(d.name) == "speed");
    REQUIRE(d.expr == expr);
    REQUIRE(d.expr->args.size() == 1);
    REQUIRE_FALSE(d.hasTriggers());
}

TEST_CASE("NodeDeclAST: `node play_sfx = PlaySound(\"hit.wav\") on on_hit`",
          "[core][ast][decl][node]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("PlaySound"));
    auto *arg = arena.make<LiteralValueAST>(
        LiteralKind::String, pool.intern("hit.wav"));
    auto args = arena.makeSpan<BaseAST *>({static_cast<BaseAST *>(arg)});
    auto *expr = arena.make<NodeExprAST>(type, args);

    auto triggers = arena.makeSpan<InternedString>({pool.intern("on_hit")});

    NodeDeclAST d{pool.intern("play_sfx"), expr, triggers};

    REQUIRE(pool.lookupView(d.name) == "play_sfx");
    REQUIRE(d.hasTriggers());
    REQUIRE(d.triggers.size() == 1);
    REQUIRE(pool.lookupView(d.triggers[0]) == "on_hit");
}

TEST_CASE("NodeDeclAST: `node damage = Damage(body, 10) on on_hit, on_other`",
          "[core][ast][decl][node]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("Damage"));
    auto *body = arena.make<IdentifierValueAST>(pool.intern("body"));
    auto *amt = arena.make<LiteralValueAST>(
        LiteralKind::Int, pool.intern("10"));
    auto args = arena.makeSpan<BaseAST *>({
        static_cast<BaseAST *>(body),
        static_cast<BaseAST *>(amt),
    });
    auto *expr = arena.make<NodeExprAST>(type, args);

    auto triggers = arena.makeSpan<InternedString>({
        pool.intern("on_hit"),
        pool.intern("on_other"),
    });

    NodeDeclAST d{pool.intern("damage"), expr, triggers};

    REQUIRE(d.expr->args.size() == 2);
    REQUIRE(d.triggers.size() == 2);
    REQUIRE(pool.lookupView(d.triggers[1]) == "on_other");
}

TEST_CASE("NodeDeclAST: a qualified node type",
          "[core][ast][decl][node]")
{
    // `node h = physics::Body(player)`
    //
    // The node type is a TypeIdAST with qualifier = "physics" and
    // name = "Body". The parser does not resolve the qualifier; Sema does.
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(
        pool.intern("physics"), pool.intern("Body"));
    auto *arg = arena.make<IdentifierValueAST>(pool.intern("player"));
    auto args = arena.makeSpan<BaseAST *>({static_cast<BaseAST *>(arg)});
    auto *expr = arena.make<NodeExprAST>(type, args);

    NodeDeclAST d{pool.intern("h"), expr};

    REQUIRE(d.expr->type->isQualified());
    REQUIRE(d.expr->type->qualifier == pool.intern("physics"));
    REQUIRE(d.expr->type->name == pool.intern("Body"));
}

TEST_CASE("NodeDeclAST: isa distinguishes it from other declarations",
          "[core][ast][decl][node]")
{
    NodeDeclAST d;
    REQUIRE_FALSE(d.isa<ImportDeclAST>());
    REQUIRE_FALSE(d.isa<EnumDeclAST>());
    REQUIRE_FALSE(d.isa<ResourceDeclAST>());
}

// ─────────────────────────────────────────────────────────────────────────────
// The family base as a common type
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("DeclAST: every concrete declaration isa DeclAST",
          "[core][ast][decl]")
{
    ImportDeclAST imp;
    EnumDeclAST en;
    ResourceDeclAST res;
    NodeDeclAST node;

    REQUIRE(imp.isa<DeclAST>());
    REQUIRE(en.isa<DeclAST>());
    REQUIRE(res.isa<DeclAST>());
    REQUIRE(node.isa<DeclAST>());
}

TEST_CASE("DeclAST: a span of the family base holds different decl kinds",
          "[core][ast][decl]")
{
    ASTArena arena;
    StringPool pool;

    auto *imp = arena.make<ImportDeclAST>(
        pool.intern("core"), pool.intern("core"));
    auto *en =
        arena.make<EnumDeclAST>(pool.intern("E"), ArenaSpan<EnumMemberAST *>{});
    auto *res = arena.make<ResourceDeclAST>(pool.intern("R"), ArenaSpan<ResourceFieldAST *>{});
    auto *node = arena.make<NodeDeclAST>(pool.intern("n"), nullptr);

    auto decls = arena.makeSpan<DeclAST *>({
        static_cast<DeclAST *>(imp),
        static_cast<DeclAST *>(en),
        static_cast<DeclAST *>(res),
        static_cast<DeclAST *>(node),
    });

    REQUIRE(decls.size() == 4);
    REQUIRE(decls[0]->isa<ImportDeclAST>());
    REQUIRE(decls[1]->isa<EnumDeclAST>());
    REQUIRE(decls[2]->isa<ResourceDeclAST>());
    REQUIRE(decls[3]->isa<NodeDeclAST>());
}