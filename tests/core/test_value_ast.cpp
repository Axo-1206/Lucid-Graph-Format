/// @file tests/core/test_value_ast.cpp
///
/// @brief Tests for ValueAST.hpp: NodeExprAST and the four value nodes.

#include "core/ast/ValueAST.hpp"
#include "core/ast/AttributeAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/ast/BaseAST.hpp"
#include "core/Tokens.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// NodeExprAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("NodeExprAST: default has the NodeExpr kind",
          "[core][ast][value][nodeexpr]")
{
    NodeExprAST n;
    REQUIRE(n.kind == ASTKind::NodeExpr);
    REQUIRE(n.isa<NodeExprAST>());
    REQUIRE(n.type == nullptr);
    REQUIRE(n.args.empty());
}

TEST_CASE("NodeExprAST: carries a type and args",
          "[core][ast][value][nodeexpr]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("Float32Node"));
    auto *arg1 = arena.make<LiteralValueAST>(
        LiteralKind::Float, pool.intern("200.0"));

    auto args = arena.makeSpan<BaseAST *>({static_cast<BaseAST *>(arg1)});
    NodeExprAST n{type, args};

    REQUIRE(n.type == type);
    REQUIRE(n.args.size() == 1);
    REQUIRE(n.args[0] == arg1);
}

TEST_CASE("NodeExprAST: no-argument node expression",
          "[core][ast][value][nodeexpr]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("EveryFrame"));
    NodeExprAST n{type, {}};

    REQUIRE(n.type == type);
    REQUIRE(n.args.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// LiteralValueAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LiteralValueAST: default kind is Int and text is invalid",
          "[core][ast][value][literal]")
{
    LiteralValueAST lit;
    REQUIRE(lit.kind == LiteralKind::Int);
    REQUIRE_FALSE(lit.text.isValid());
}

TEST_CASE("LiteralValueAST: integer literal",
          "[core][ast][value][literal]")
{
    StringPool pool;
    LiteralValueAST lit{LiteralKind::Int, pool.intern("42")};

    REQUIRE(lit.kind == LiteralKind::Int);
    REQUIRE(pool.lookupView(lit.text) == "42");
}

TEST_CASE("LiteralValueAST: hex integer keeps its radix prefix",
          "[core][ast][value][literal]")
{
    StringPool pool;
    LiteralValueAST lit{LiteralKind::Int, pool.intern("0xFF")};
    REQUIRE(pool.lookupView(lit.text) == "0xFF");
}

TEST_CASE("LiteralValueAST: negative integer keeps its sign",
          "[core][ast][value][literal]")
{
    StringPool pool;
    LiteralValueAST lit{LiteralKind::Int, pool.intern("-7")};
    REQUIRE(pool.lookupView(lit.text) == "-7");
}

TEST_CASE("LiteralValueAST: negative float keeps its sign",
          "[core][ast][value][literal]")
{
    StringPool pool;
    LiteralValueAST lit{LiteralKind::Float, pool.intern("-400.0")};
    REQUIRE(pool.lookupView(lit.text) == "-400.0");
}

TEST_CASE("LiteralValueAST: explicit plus sign is preserved",
          "[core][ast][value][literal]")
{
    StringPool pool;
    LiteralValueAST lit{LiteralKind::Int, pool.intern("+7")};
    REQUIRE(pool.lookupView(lit.text) == "+7");
}

TEST_CASE("LiteralValueAST: float literal",
          "[core][ast][value][literal]")
{
    StringPool pool;
    LiteralValueAST lit{LiteralKind::Float, pool.intern("3.14")};

    REQUIRE(lit.kind == LiteralKind::Float);
    REQUIRE(pool.lookupView(lit.text) == "3.14");
}

TEST_CASE("LiteralValueAST: string literal stores the resolved content",
          "[core][ast][value][literal]")
{
    StringPool pool;
    // The lexer resolves escapes. The AST stores the resolved bytes.
    // The formatter re-escapes on output.
    LiteralValueAST lit{LiteralKind::String, pool.intern("hello\nworld")};

    REQUIRE(lit.kind == LiteralKind::String);
    REQUIRE(pool.lookupView(lit.text) == "hello\nworld");
}

TEST_CASE("LiteralValueAST: char literal stores the resolved character",
          "[core][ast][value][literal]")
{
    StringPool pool;
    LiteralValueAST lit{LiteralKind::Char, pool.intern("\n")};

    REQUIRE(lit.kind == LiteralKind::Char);
    REQUIRE(pool.lookupView(lit.text) == "\n");
}

TEST_CASE("LiteralValueAST: bool literal",
          "[core][ast][value][literal]")
{
    StringPool pool;
    LiteralValueAST t{LiteralKind::Bool, pool.intern("true")};
    LiteralValueAST f{LiteralKind::Bool, pool.intern("false")};

    REQUIRE(t.kind == LiteralKind::Bool);
    REQUIRE(pool.lookupView(t.text) == "true");
    REQUIRE(pool.lookupView(f.text) == "false");
}

TEST_CASE("LiteralValueAST: nil literal",
          "[core][ast][value][literal]")
{
    StringPool pool;
    LiteralValueAST lit{LiteralKind::Nil, pool.intern("nil")};

    REQUIRE(lit.kind == LiteralKind::Nil);
    REQUIRE(pool.lookupView(lit.text) == "nil");
}

TEST_CASE("LiteralValueAST: isa and as",
          "[core][ast][value][literal]")
{
    StringPool pool;
    LiteralValueAST lit{LiteralKind::Int, pool.intern("1")};

    REQUIRE(lit.isa<LiteralValueAST>());
    REQUIRE_FALSE(lit.isa<IdentifierValueAST>());
    REQUIRE_FALSE(lit.isa<FieldAccessValueAST>());
    REQUIRE_FALSE(lit.isa<InlineNodeValueAST>());
    REQUIRE_FALSE(lit.isa<NodeExprAST>());
    REQUIRE_FALSE(lit.isa<AttributeAST>());

    BaseAST *base = &lit;
    REQUIRE(base->isa<LiteralValueAST>());
    REQUIRE(base->as<LiteralValueAST>()->kind == LiteralKind::Int);
}

// ─────────────────────────────────────────────────────────────────────────────
// IdentifierValueAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IdentifierValueAST: default",
          "[core][ast][value][identifier]")
{
    IdentifierValueAST id;
    REQUIRE(id.kind == ASTKind::IdentifierValue);
    REQUIRE_FALSE(id.name.isValid());
}

TEST_CASE("IdentifierValueAST: carries a name",
          "[core][ast][value][identifier]")
{
    StringPool pool;
    IdentifierValueAST id{pool.intern("player")};

    REQUIRE(id.isa<IdentifierValueAST>());
    REQUIRE(pool.lookupView(id.name) == "player");
}

TEST_CASE("IdentifierValueAST: isa distinguishes it from the other forms",
          "[core][ast][value][identifier]")
{
    StringPool pool;
    IdentifierValueAST id{pool.intern("x")};

    REQUIRE_FALSE(id.isa<LiteralValueAST>());
    REQUIRE_FALSE(id.isa<FieldAccessValueAST>());
    REQUIRE_FALSE(id.isa<InlineNodeValueAST>());
}

// ─────────────────────────────────────────────────────────────────────────────
// FieldAccessValueAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("FieldAccessValueAST: default",
          "[core][ast][value][fieldaccess]")
{
    FieldAccessValueAST fa;
    REQUIRE(fa.kind == ASTKind::FieldAccessValue);
    REQUIRE_FALSE(fa.object.isValid());
    REQUIRE_FALSE(fa.field.isValid());
}

TEST_CASE("FieldAccessValueAST: carries both parts",
          "[core][ast][value][fieldaccess]")
{
    StringPool pool;
    FieldAccessValueAST fa{pool.intern("Config"), pool.intern("speed")};

    REQUIRE(pool.lookupView(fa.object) == "Config");
    REQUIRE(pool.lookupView(fa.field) == "speed");
}

TEST_CASE("FieldAccessValueAST: a real-world example",
          "[core][ast][value][fieldaccess]")
{
    // `player_health.current` from the grammar's §2.7 example.
    StringPool pool;
    FieldAccessValueAST fa{pool.intern("player_health"),
                           pool.intern("current")};

    REQUIRE(pool.lookupView(fa.object) == "player_health");
    REQUIRE(pool.lookupView(fa.field) == "current");
}

TEST_CASE("FieldAccessValueAST: enum member access",
          "[core][ast][value][fieldaccess]")
{
    // `Key.A` from the grammar's §2.5 example.
    StringPool pool;
    FieldAccessValueAST fa{pool.intern("Key"), pool.intern("A")};

    REQUIRE(pool.lookupView(fa.object) == "Key");
    REQUIRE(pool.lookupView(fa.field) == "A");
}

TEST_CASE("FieldAccessValueAST: isa distinguishes it from the other forms",
          "[core][ast][value][fieldaccess]")
{
    StringPool pool;
    FieldAccessValueAST fa{pool.intern("a"), pool.intern("b")};

    REQUIRE_FALSE(fa.isa<LiteralValueAST>());
    REQUIRE_FALSE(fa.isa<IdentifierValueAST>());
    REQUIRE_FALSE(fa.isa<InlineNodeValueAST>());
}

// ─────────────────────────────────────────────────────────────────────────────
// InlineNodeValueAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("InlineNodeValueAST: default",
          "[core][ast][value][inline]")
{
    InlineNodeValueAST in;
    REQUIRE(in.kind == ASTKind::InlineNodeValue);
    REQUIRE(in.node == nullptr);
}

TEST_CASE("InlineNodeValueAST: wraps a NodeExprAST",
          "[core][ast][value][inline]")
{
    ASTArena arena;
    StringPool pool;

    auto *type = arena.make<TypeIdAST>(pool.intern("Float32Node"));
    auto *arg = arena.make<LiteralValueAST>(
        LiteralKind::Float, pool.intern("200.0"));
    auto args = arena.makeSpan<BaseAST *>({static_cast<BaseAST *>(arg)});
    auto *expr = arena.make<NodeExprAST>(type, args);

    InlineNodeValueAST in{expr};

    REQUIRE(in.isa<InlineNodeValueAST>());
    REQUIRE(in.node == expr);
    REQUIRE(in.node->args.size() == 1);
}

TEST_CASE("InlineNodeValueAST: isa distinguishes it from the other forms",
          "[core][ast][value][inline]")
{
    InlineNodeValueAST in;
    REQUIRE_FALSE(in.isa<LiteralValueAST>());
    REQUIRE_FALSE(in.isa<IdentifierValueAST>());
    REQUIRE_FALSE(in.isa<FieldAccessValueAST>());
    REQUIRE_FALSE(in.isa<NodeExprAST>());
}

// ─────────────────────────────────────────────────────────────────────────────
// Recursive structure: a node expression containing value arguments
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ValueAST: nested inline nodes",
          "[core][ast][value][recursive]")
{
    // Model the grammar's example `DivideNode(State.current, State.max)`:
    //
    //   InlineNodeValueAST
    //     └── NodeExprAST("DivideNode")
    //           ├── FieldAccessValueAST("State", "current")
    //           └── FieldAccessValueAST("State", "max")
    ASTArena arena;
    StringPool pool;

    auto *stateCurrent = arena.make<FieldAccessValueAST>(
        pool.intern("State"), pool.intern("current"));
    auto *stateMax = arena.make<FieldAccessValueAST>(
        pool.intern("State"), pool.intern("max"));

    auto args = arena.makeSpan<BaseAST *>({
        static_cast<BaseAST *>(stateCurrent),
        static_cast<BaseAST *>(stateMax),
    });

    auto *type = arena.make<TypeIdAST>(pool.intern("DivideNode"));
    auto *expr = arena.make<NodeExprAST>(type, args);
    auto *inlineValue = arena.make<InlineNodeValueAST>(expr);

    REQUIRE(inlineValue->isa<InlineNodeValueAST>());
    REQUIRE(inlineValue->node->isa<NodeExprAST>());
    REQUIRE(inlineValue->node->type->name == pool.intern("DivideNode"));
    REQUIRE(inlineValue->node->args.size() == 2);

    BaseAST *a0 = inlineValue->node->args[0];
    REQUIRE(a0->isa<FieldAccessValueAST>());
    REQUIRE(a0->as<FieldAccessValueAST>()->object == pool.intern("State"));
    REQUIRE(a0->as<FieldAccessValueAST>()->field == pool.intern("current"));

    BaseAST *a1 = inlineValue->node->args[1];
    REQUIRE(a1->isa<FieldAccessValueAST>());
    REQUIRE(a1->as<FieldAccessValueAST>()->field == pool.intern("max"));
}

TEST_CASE("ValueAST: a mixed argument list",
          "[core][ast][value][recursive]")
{
    // Arguments of different kinds in one node expression:
    //   `Damage(body, 10)`  ->  IdentifierValueAST, LiteralValueAST
    ASTArena arena;
    StringPool pool;

    auto *body = arena.make<IdentifierValueAST>(pool.intern("body"));
    auto *amount = arena.make<LiteralValueAST>(
        LiteralKind::Int, pool.intern("10"));

    auto args = arena.makeSpan<BaseAST *>({
        static_cast<BaseAST *>(body),
        static_cast<BaseAST *>(amount),
    });

    auto *type = arena.make<TypeIdAST>(pool.intern("Damage"));
    auto *expr = arena.make<NodeExprAST>(type, args);

    REQUIRE(expr->args.size() == 2);
    REQUIRE(expr->args[0]->isa<IdentifierValueAST>());
    REQUIRE(expr->args[1]->isa<LiteralValueAST>());
    REQUIRE(expr->args[1]->as<LiteralValueAST>()->kind == LiteralKind::Int);
}