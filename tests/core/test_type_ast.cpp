/// @file tests/core/test_type_ast.cpp
///
/// @brief Tests for TypeIdAST.

#include "core/ast/TypeAST.hpp"
#include "core/ast/BaseAST.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TypeIdAST: default has the TypeId kind",
          "[core][ast][type]")
{
    TypeIdAST t;
    REQUIRE(t.kind == ASTKind::TypeId);
    REQUIRE(t.isa<TypeIdAST>());
}

TEST_CASE("TypeIdAST: default fields are invalid",
          "[core][ast][type]")
{
    TypeIdAST t;
    REQUIRE_FALSE(t.qualifier.isValid());
    REQUIRE_FALSE(t.name.isValid());
}

// ─────────────────────────────────────────────────────────────────────────────
// Simple (unqualified) type references
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TypeIdAST: simple type carries only a name",
          "[core][ast][type]")
{
    StringPool pool;
    InternedString name = pool.intern("Key");

    TypeIdAST t{name};

    REQUIRE(t.name == name);
    REQUIRE(pool.lookupView(t.name) == "Key");
    REQUIRE_FALSE(t.qualifier.isValid());
}

TEST_CASE("TypeIdAST: isSimple is true for an unqualified reference",
          "[core][ast][type]")
{
    StringPool pool;
    TypeIdAST t{pool.intern("Key")};

    REQUIRE(t.isSimple());
    REQUIRE_FALSE(t.isQualified());
}

// ─────────────────────────────────────────────────────────────────────────────
// Qualified type references
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TypeIdAST: qualified type carries both parts",
          "[core][ast][type]")
{
    StringPool pool;
    InternedString q = pool.intern("core");
    InternedString n = pool.intern("Key");

    TypeIdAST t{q, n};

    REQUIRE(t.qualifier == q);
    REQUIRE(t.name == n);
    REQUIRE(pool.lookupView(t.qualifier) == "core");
    REQUIRE(pool.lookupView(t.name) == "Key");
}

TEST_CASE("TypeIdAST: isQualified is true for a qualified reference",
          "[core][ast][type]")
{
    StringPool pool;
    TypeIdAST t{pool.intern("core"), pool.intern("Key")};

    REQUIRE(t.isQualified());
    REQUIRE_FALSE(t.isSimple());
}

TEST_CASE("TypeIdAST: a qualified reference with an invalid qualifier is simple",
          "[core][ast][type]")
{
    // Constructing with an invalid qualifier produces a node that behaves
    // like a simple reference. This is what the two-argument constructor
    // does when the caller passes an empty qualifier; the accessor reads
    // the same information.
    StringPool pool;
    TypeIdAST t{InternedString{}, pool.intern("Key")};

    REQUIRE(t.isSimple());
    REQUIRE_FALSE(t.isQualified());
    REQUIRE(pool.lookupView(t.name) == "Key");
}

// ─────────────────────────────────────────────────────────────────────────────
// isa / as
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TypeIdAST: isa returns true for TypeIdAST",
          "[core][ast][type]")
{
    TypeIdAST t;
    REQUIRE(t.isa<TypeIdAST>());
}

TEST_CASE("TypeIdAST: isa returns false for other kinds",
          "[core][ast][type]")
{
    TypeIdAST t;
    REQUIRE_FALSE(t.isa<AttributeAST>());
    REQUIRE_FALSE(t.isa<UnknownAST>());
}

TEST_CASE("TypeIdAST: as through a BaseAST pointer works",
          "[core][ast][type]")
{
    StringPool pool;
    InternedString q = pool.intern("core");
    InternedString n = pool.intern("Key");
    TypeIdAST concrete{q, n};

    BaseAST *base = &concrete;
    REQUIRE(base->isa<TypeIdAST>());
    REQUIRE(base->as<TypeIdAST>()->qualifier == q);
    REQUIRE(base->as<TypeIdAST>()->name == n);
}

// ─────────────────────────────────────────────────────────────────────────────
// Interning
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TypeIdAST: identical type names share interned handles",
          "[core][ast][type]")
{
    StringPool pool;

    TypeIdAST a{pool.intern("Key")};
    TypeIdAST b{pool.intern("Key")};

    REQUIRE(a.name == b.name);
    REQUIRE(a.name.id == b.name.id);
}

TEST_CASE("TypeIdAST: qualified and simple references to the same name differ",
          "[core][ast][type]")
{
    StringPool pool;

    TypeIdAST simple{pool.intern("Key")};
    TypeIdAST qualified{pool.intern("core"), pool.intern("Key")};

    // The `name` field is the same in both.
    REQUIRE(simple.name == qualified.name);
    // But only the qualified one has a qualifier.
    REQUIRE(simple.isSimple());
    REQUIRE(qualified.isQualified());
}

// ─────────────────────────────────────────────────────────────────────────────
// Realistic examples from the grammar
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TypeIdAST: the grammar's `Key` example",
          "[core][ast][type]")
{
    StringPool pool;
    TypeIdAST t{pool.intern("Key")};
    REQUIRE(pool.lookupView(t.name) == "Key");
    REQUIRE(t.isSimple());
}

TEST_CASE("TypeIdAST: the grammar's `core.Key` example",
          "[core][ast][type]")
{
    StringPool pool;
    TypeIdAST t{pool.intern("core"), pool.intern("Key")};
    REQUIRE(pool.lookupView(t.qualifier) == "core");
    REQUIRE(pool.lookupView(t.name) == "Key");
    REQUIRE(t.isQualified());
}

TEST_CASE("TypeIdAST: a resource field's type annotation",
          "[core][ast][type]")
{
    // `speed: float` produces a TypeIdAST with name = "float".
    StringPool pool;
    TypeIdAST t{pool.intern("float")};
    REQUIRE(pool.lookupView(t.name) == "float");
    REQUIRE(t.isSimple());
}