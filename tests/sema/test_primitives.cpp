/// @file tests/sema/test_primitives.cpp
///
/// @brief Tests for the primitive alias table.

#include "sema/Primitives.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lucid::sema;

TEST_CASE("normalizePrimitiveName maps aliases to canonical names",
          "[sema][primitives]")
{
    CHECK(normalizePrimitiveName("float")  == "float32");
    CHECK(normalizePrimitiveName("double") == "float64");
    CHECK(normalizePrimitiveName("int")    == "int32");
    CHECK(normalizePrimitiveName("uint")   == "uint32");
    CHECK(normalizePrimitiveName("byte")   == "uint8");
    CHECK(normalizePrimitiveName("short")  == "int16");
    CHECK(normalizePrimitiveName("long")   == "int64");
}

TEST_CASE("normalizePrimitiveName is idempotent on canonical names",
          "[sema][primitives]")
{
    CHECK(normalizePrimitiveName("float32") == "float32");
    CHECK(normalizePrimitiveName("int64")   == "int64");
    CHECK(normalizePrimitiveName("string")  == "string");
}

TEST_CASE("normalizePrimitiveName returns unknown names unchanged",
          "[sema][primitives]")
{
    CHECK(normalizePrimitiveName("Key")     == "Key");
    CHECK(normalizePrimitiveName("BodyRef") == "BodyRef");
    CHECK(normalizePrimitiveName("")        == "");
}

TEST_CASE("isCanonicalPrimitive recognizes all primitive names",
          "[sema][primitives]")
{
    CHECK(isCanonicalPrimitive("bool"));
    CHECK(isCanonicalPrimitive("char"));
    CHECK(isCanonicalPrimitive("string"));
    CHECK(isCanonicalPrimitive("int8"));
    CHECK(isCanonicalPrimitive("int16"));
    CHECK(isCanonicalPrimitive("int32"));
    CHECK(isCanonicalPrimitive("int64"));
    CHECK(isCanonicalPrimitive("uint8"));
    CHECK(isCanonicalPrimitive("uint16"));
    CHECK(isCanonicalPrimitive("uint32"));
    CHECK(isCanonicalPrimitive("uint64"));
    CHECK(isCanonicalPrimitive("float32"));
    CHECK(isCanonicalPrimitive("float64"));
    CHECK_FALSE(isCanonicalPrimitive("float"));
    CHECK_FALSE(isCanonicalPrimitive("Key"));
    CHECK_FALSE(isCanonicalPrimitive(""));
}

TEST_CASE("primitiveLiteralKind maps canonical names to literal kinds",
          "[sema][primitives]")
{
    CHECK(primitiveLiteralKind("bool")    == Literal::Kind::Bool);
    CHECK(primitiveLiteralKind("char")    == Literal::Kind::Char);
    CHECK(primitiveLiteralKind("string")  == Literal::Kind::String);
    CHECK(primitiveLiteralKind("int32")   == Literal::Kind::Int32);
    CHECK(primitiveLiteralKind("float32") == Literal::Kind::Float32);
    CHECK(primitiveLiteralKind("Key")     == Literal::Kind::Nil);
}
