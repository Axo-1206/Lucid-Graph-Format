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

TEST_CASE("ImportDeclAST: `import core.keys as k`",
          "[core][ast][decl][import]")
{
    StringPool pool;
    ImportDeclAST d{pool.intern("core.keys"), pool.intern("k")};

    REQUIRE(pool.lookupView(d.path) == "core.keys");
    REQUIRE(pool.lookupView(d.name) == "k");
}

TEST_CASE("ImportDeclAST: isa distinguishes it from other declarations",
          "[core][ast][decl][import]")
{
    ImportDeclAST d;
    REQUIRE_FALSE(d.isa<EnumDeclAST>());
    REQUIRE_FALSE(d.isa<ResourceDeclAST>());
    REQUIRE_FALSE(d.isa<NodeDeclAST>());
    REQUIRE_FALSE(d.isa<CompositeDeclAST>());
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

    auto members = arena.makeSpan<InternedString>({
        pool.intern("A"),
        pool.intern("B"),
        pool.intern("C"),
    });

    EnumDeclAST d{pool.intern("Key"), members};

    REQUIRE(pool.lookupView(d.name) == "Key");
    REQUIRE(d.members.size() == 3);
    REQUIRE(pool.lookupView(d.members[0]) == "A");
    REQUIRE(pool.lookupView(d.members[1]) == "B");
    REQUIRE(pool.lookupView(d.members[2]) == "C");
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
    REQUIRE_FALSE(d.isa<CompositeDeclAST>());
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
    REQUIRE(f.defaultValue->kind == LiteralKind::Float);
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
    REQUIRE(f.defaultValue->kind == LiteralKind::String);
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
    REQUIRE_FALSE(d.isa<CompositeDeclAST>());
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

TEST_CASE("NodeDeclAST: isa distinguishes it from other declarations",
          "[core][ast][decl][node]")
{
    NodeDeclAST d;
    REQUIRE_FALSE(d.isa<ImportDeclAST>());
    REQUIRE_FALSE(d.isa<EnumDeclAST>());
    REQUIRE_FALSE(d.isa<ResourceDeclAST>());
    REQUIRE_FALSE(d.isa<CompositeDeclAST>());
}

// ─────────────────────────────────────────────────────────────────────────────
// CompositeInputAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CompositeInputAST: default",
          "[core][ast][decl][input]")
{
    CompositeInputAST in;
    REQUIRE(in.kind == ASTKind::CompositeInput);
    REQUIRE(in.isa<CompositeInputAST>());
    REQUIRE(in.type == nullptr);
}

TEST_CASE("CompositeInputAST: `max: int`",
          "[core][ast][decl][input]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("int"));
    CompositeInputAST in{pool.intern("max"), type};

    REQUIRE(pool.lookupView(in.name) == "max");
    REQUIRE(in.type == type);
}

// ─────────────────────────────────────────────────────────────────────────────
// CompositeOutputAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CompositeOutputAST: default",
          "[core][ast][decl][output]")
{
    CompositeOutputAST out;
    REQUIRE(out.kind == ASTKind::CompositeOutput);
    REQUIRE(out.isa<CompositeOutputAST>());
    REQUIRE(out.type == nullptr);
    REQUIRE(out.value == nullptr);
}

TEST_CASE("CompositeOutputAST: `current: int = State.current`",
          "[core][ast][decl][output]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("int"));
    auto *value = arena.make<FieldAccessValueAST>(
        pool.intern("State"), pool.intern("current"));

    CompositeOutputAST out{pool.intern("current"), type, value};

    REQUIRE(pool.lookupView(out.name) == "current");
    REQUIRE(out.type == type);
    REQUIRE(out.value == value);
    REQUIRE(out.value->isa<FieldAccessValueAST>());
}

TEST_CASE("CompositeOutputAST: `on_death: Event = dead`",
          "[core][ast][decl][output]")
{
    // An event output. The right-hand side is an identifier referring to
    // a trigger node inside the body.
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("Event"));
    auto *value = arena.make<IdentifierValueAST>(pool.intern("dead"));

    CompositeOutputAST out{pool.intern("on_death"), type, value};

    REQUIRE(pool.lookupView(out.type->name) == "Event");
    REQUIRE(out.value->isa<IdentifierValueAST>());
    REQUIRE(pool.lookupView(out.value->as<IdentifierValueAST>()->name) == "dead");
}

// ─────────────────────────────────────────────────────────────────────────────
// CompositeDeclAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CompositeDeclAST: default",
          "[core][ast][decl][composite]")
{
    CompositeDeclAST d;
    REQUIRE(d.kind == ASTKind::CompositeDecl);
    REQUIRE(d.isa<CompositeDeclAST>());
    REQUIRE(d.isa<DeclAST>());
    REQUIRE(d.inputs.empty());
    REQUIRE(d.outputs.empty());
    REQUIRE(d.body.empty());
    REQUIRE_FALSE(d.hasInputs());
    REQUIRE_FALSE(d.hasOutputs());
}

TEST_CASE("CompositeDeclAST: the grammar's Health example",
          "[core][ast][decl][composite]")
{
    // A trimmed version of the grammar's §2.7 Health composite:
    //
    //     composite Health {
    //         input  { max: int }
    //         output { current: int = State.current }
    //         resource State { current: int = 0 }
    //     }
    ASTArena arena;
    StringPool pool;

    // Input: `max: int`
    auto *intType = arena.make<TypeIdAST>(pool.intern("int"));
    auto *maxIn = arena.make<CompositeInputAST>(pool.intern("max"), intType);
    auto inputs = arena.makeSpan<CompositeInputAST *>({maxIn});

    // Output: `current: int = State.current`
    auto *currentFA = arena.make<FieldAccessValueAST>(
        pool.intern("State"), pool.intern("current"));
    auto *currentOut = arena.make<CompositeOutputAST>(
        pool.intern("current"), intType, currentFA);
    auto outputs = arena.makeSpan<CompositeOutputAST *>({currentOut});

    // Body: `resource State { current: int = 0 }`
    auto *zeroLit = arena.make<LiteralValueAST>(
        LiteralKind::Int, pool.intern("0"));
    auto *currentField = arena.make<ResourceFieldAST>(
        pool.intern("current"), intType, zeroLit);
    auto stateFields = arena.makeSpan<ResourceFieldAST *>({currentField});
    auto *stateDecl = arena.make<ResourceDeclAST>(
        pool.intern("State"), stateFields);
    auto body = arena.makeSpan<DeclAST *>({stateDecl});

    CompositeDeclAST d{pool.intern("Health"), inputs, outputs, body};

    REQUIRE(pool.lookupView(d.name) == "Health");
    REQUIRE(d.hasInputs());
    REQUIRE(d.hasOutputs());
    REQUIRE(d.inputs.size() == 1);
    REQUIRE(d.outputs.size() == 1);
    REQUIRE(d.body.size() == 1);

    REQUIRE(pool.lookupView(d.inputs[0]->name) == "max");
    REQUIRE(pool.lookupView(d.outputs[0]->name) == "current");
    REQUIRE(d.body[0]->isa<ResourceDeclAST>());
    REQUIRE(pool.lookupView(d.body[0]->as<ResourceDeclAST>()->name) == "State");
}

TEST_CASE("CompositeDeclAST: with attributes",
          "[core][ast][decl][composite]")
{
    ASTArena arena;
    StringPool pool;

    auto *exportAttr = arena.make<AttributeAST>(pool.intern("export"));
    auto attrs = arena.makeSpan<AttributeAST *>({exportAttr});

    CompositeDeclAST d{pool.intern("Health"), {}, {}, {}, attrs};

    REQUIRE(d.attributes.size() == 1);
    REQUIRE(pool.lookupView(d.attributes[0]->name) == "export");
}

TEST_CASE("CompositeDeclAST: isa distinguishes it from other declarations",
          "[core][ast][decl][composite]")
{
    CompositeDeclAST d;
    REQUIRE_FALSE(d.isa<ImportDeclAST>());
    REQUIRE_FALSE(d.isa<EnumDeclAST>());
    REQUIRE_FALSE(d.isa<ResourceDeclAST>());
    REQUIRE_FALSE(d.isa<NodeDeclAST>());
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
    CompositeDeclAST comp;

    REQUIRE(imp.isa<DeclAST>());
    REQUIRE(en.isa<DeclAST>());
    REQUIRE(res.isa<DeclAST>());
    REQUIRE(node.isa<DeclAST>());
    REQUIRE(comp.isa<DeclAST>());
}

TEST_CASE("DeclAST: a span of the family base holds different decl kinds",
          "[core][ast][decl]")
{
    ASTArena arena;
    StringPool pool;

    auto *imp = arena.make<ImportDeclAST>(
        pool.intern("core"), pool.intern("core"));
    auto *en = arena.make<EnumDeclAST>(pool.intern("E"), ArenaSpan<InternedString>{});
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