/// @file tests/core/test_base_ast.cpp
///
/// @brief Tests for BaseAST, ASTKind, isa/as, and UnknownAST.

#include "core/ast/BaseAST.hpp"

#include <catch2/catch_test_macros.hpp>

namespace
{

    // A minimal concrete node for testing the isa/as machinery. When the real
    // AST nodes land (steps 3.2 through 3.6), this local test node is no
    // longer necessary, but the tests below still exercise the base-class
    // contract.
    struct TestNodeAST : BaseAST
    {
        static constexpr ASTKind staticKind = ASTKind::Attribute;

        int value = 0;

        TestNodeAST() : BaseAST(ASTKind::Attribute) {}
        explicit TestNodeAST(int v) : BaseAST(ASTKind::Attribute), value(v) {}
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// ASTKind
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ASTKind: every concrete node has a distinct tag",
          "[core][ast][kind]")
{
    // A handful of representative comparisons. The exact numeric values
    // do not matter; the distinctness does.
    REQUIRE(ASTKind::Attribute != ASTKind::TypeId);
    REQUIRE(ASTKind::TypeId != ASTKind::LiteralValue);
    REQUIRE(ASTKind::LiteralValue != ASTKind::IdentifierValue);
    REQUIRE(ASTKind::ImportDecl != ASTKind::NodeDecl);
    REQUIRE(ASTKind::Module != ASTKind::Unknown);
}

// ─────────────────────────────────────────────────────────────────────────────
// BaseAST construction
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("BaseAST: kind is set by the constructor",
          "[core][ast]")
{
    TestNodeAST node;
    REQUIRE(node.kind == ASTKind::Attribute);
}

TEST_CASE("BaseAST: default location is unknown",
          "[core][ast]")
{
    TestNodeAST node;
    REQUIRE_FALSE(node.loc.isKnown());
}

TEST_CASE("BaseAST: hasSyntaxError defaults to false",
          "[core][ast]")
{
    TestNodeAST node;
    REQUIRE_FALSE(node.hasSyntaxError);
}

// ─────────────────────────────────────────────────────────────────────────────
// isa / as
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("BaseAST: isa returns true for the concrete type",
          "[core][ast][isa]")
{
    TestNodeAST node;
    REQUIRE(node.isa<TestNodeAST>());
}

TEST_CASE("BaseAST: isa returns false for a different type",
          "[core][ast][isa]")
{
    TestNodeAST node;
    REQUIRE_FALSE(node.isa<UnknownAST>());
}

TEST_CASE("BaseAST: as returns a pointer to the concrete type",
          "[core][ast][as]")
{
    TestNodeAST node{42};
    TestNodeAST *p = node.as<TestNodeAST>();
    REQUIRE(p == &node);
    REQUIRE(p->value == 42);
}

TEST_CASE("BaseAST: as on a const node returns a const pointer",
          "[core][ast][as]")
{
    const TestNodeAST node{42};
    const TestNodeAST *p = node.as<TestNodeAST>();
    REQUIRE(p == &node);
    REQUIRE(p->value == 42);
}

TEST_CASE("BaseAST: as through a BaseAST pointer preserves the concrete type",
          "[core][ast][as]")
{
    TestNodeAST concrete{7};
    BaseAST *base = &concrete;

    REQUIRE(base->isa<TestNodeAST>());
    REQUIRE(base->as<TestNodeAST>()->value == 7);
}

// ─────────────────────────────────────────────────────────────────────────────
// UnknownAST
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("UnknownAST: has the Unknown kind", "[core][ast][unknown]")
{
    UnknownAST node;
    REQUIRE(node.kind == ASTKind::Unknown);
}

TEST_CASE("UnknownAST: hasSyntaxError is true",
          "[core][ast][unknown]")
{
    UnknownAST node;
    REQUIRE(node.hasSyntaxError);
}

TEST_CASE("UnknownAST: isa and as work", "[core][ast][unknown]")
{
    UnknownAST node;
    REQUIRE(node.isa<UnknownAST>());
    REQUIRE(node.as<UnknownAST>() == &node);
}

// ─────────────────────────────────────────────────────────────────────────────
// isUnknown
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("isUnknown: true for nullptr", "[core][ast][unknown]")
{
    REQUIRE(isUnknown(nullptr));
}

TEST_CASE("isUnknown: true for UnknownAST", "[core][ast][unknown]")
{
    UnknownAST node;
    REQUIRE(isUnknown(&node));
}

TEST_CASE("isUnknown: false for a real node", "[core][ast][unknown]")
{
    TestNodeAST node;
    REQUIRE_FALSE(isUnknown(&node));
}

// ─────────────────────────────────────────────────────────────────────────────
// The AST_ASSERT_MSG macro
// ─────────────────────────────────────────────────────────────────────────────
//
// The macro aborts the process on failure. A positive test — that it
// fires — would end the test process. It is not exercised here. A build
// that compiles the macro's body and the test executable linking
// successfully is itself the check that the macro is syntactically valid.

TEST_CASE("AST_ASSERT_MSG: does not fire on a true condition",
          "[core][ast][assert]")
{
    // The macro is a no-op when the condition is true. This test's mere
    // completion proves the macro does not fire spuriously.
    AST_ASSERT_MSG(1 + 1 == 2, "arithmetic is broken");
    REQUIRE(true);
}