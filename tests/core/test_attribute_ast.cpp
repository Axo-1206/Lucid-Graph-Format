/// @file tests/core/test_attribute_ast.cpp
///
/// @brief Tests for AttributeAST.

#include "core/ast/AttributeAST.hpp"
#include "core/ast/BaseAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("AttributeAST: default has the Attribute kind",
          "[core][ast][attribute]")
{
    AttributeAST attr;
    REQUIRE(attr.kind == ASTKind::Attribute);
    REQUIRE(attr.isa<AttributeAST>());
}

TEST_CASE("AttributeAST: default name is invalid",
          "[core][ast][attribute]")
{
    AttributeAST attr;
    REQUIRE_FALSE(attr.name.isValid());
}

TEST_CASE("AttributeAST: name is stored",
          "[core][ast][attribute]")
{
    StringPool pool;
    InternedString name = pool.intern("export");

    AttributeAST attr{name};

    REQUIRE(attr.name == name);
    REQUIRE(attr.name.isValid());
    REQUIRE(pool.lookupView(attr.name) == "export");
}

TEST_CASE("AttributeAST: hasSyntaxError defaults to false",
          "[core][ast][attribute]")
{
    AttributeAST attr;
    REQUIRE_FALSE(attr.hasSyntaxError);
}

TEST_CASE("AttributeAST: location defaults to unknown",
          "[core][ast][attribute]")
{
    AttributeAST attr;
    REQUIRE_FALSE(attr.loc.isKnown());
}

TEST_CASE("AttributeAST: location can be set",
          "[core][ast][attribute]")
{
    AttributeAST attr;
    attr.loc = SourceLocation{5, 10};

    REQUIRE(attr.loc.line() == 5);
    REQUIRE(attr.loc.column() == 10);
}

// ─────────────────────────────────────────────────────────────────────────────
// isa / as
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("AttributeAST: isa returns true for AttributeAST",
          "[core][ast][attribute]")
{
    AttributeAST attr;
    REQUIRE(attr.isa<AttributeAST>());
}

TEST_CASE("AttributeAST: isa returns false for other kinds",
          "[core][ast][attribute]")
{
    AttributeAST attr;
    REQUIRE_FALSE(attr.isa<UnknownAST>());
    REQUIRE_FALSE(attr.isa<TypeIdAST>());
}

TEST_CASE("AttributeAST: as through a BaseAST pointer works",
          "[core][ast][attribute]")
{
    StringPool pool;
    InternedString name = pool.intern("export");
    AttributeAST concrete{name};

    BaseAST *base = &concrete;
    REQUIRE(base->isa<AttributeAST>());
    REQUIRE(base->as<AttributeAST>()->name == name);
}

// ─────────────────────────────────────────────────────────────────────────────
// Realistic: `@export` on a resource
// ─────────────────────────────────────────────────────────────────────────────
//
// This test mirrors what the parser will do in step 4: it combines an
// `@` and an `IDENTIFIER("export")` into one AttributeAST. The AST node
// itself does not know where it is attached; that is the enclosing
// declaration's job.

TEST_CASE("AttributeAST: the `@export` prefix",
          "[core][ast][attribute]")
{
    StringPool pool;
    InternedString name = pool.intern("export");

    AttributeAST exportAttr{name};
    exportAttr.loc = SourceLocation{1, 1}; // the `@` sign

    REQUIRE(pool.lookupView(exportAttr.name) == "export");
    REQUIRE(exportAttr.loc.column() == 1);
}

TEST_CASE("AttributeAST: an unknown attribute is still a node",
          "[core][ast][attribute]")
{
    // The parser does not reject `@whatever`. It produces a node; Sema
    // reports the unknown name later.
    StringPool pool;
    InternedString name = pool.intern("whatever");

    AttributeAST attr{name};

    REQUIRE(attr.kind == ASTKind::Attribute);
    REQUIRE(pool.lookupView(attr.name) == "whatever");
}

// ─────────────────────────────────────────────────────────────────────────────
// Interning: identical names share the same handle
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("AttributeAST: two `@export` nodes share the interned handle",
          "[core][ast][attribute]")
{
    StringPool pool;

    AttributeAST a{pool.intern("export")};
    AttributeAST b{pool.intern("export")};

    REQUIRE(a.name == b.name);
    REQUIRE(a.name.id == b.name.id);
}

TEST_CASE("AttributeAST: distinct names have distinct handles",
          "[core][ast][attribute]")
{
    StringPool pool;

    AttributeAST a{pool.intern("export")};
    AttributeAST b{pool.intern("whatever")};

    REQUIRE(a.name != b.name);
}