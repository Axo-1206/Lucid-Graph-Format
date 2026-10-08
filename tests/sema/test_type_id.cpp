/// @file tests/sema/test_type_id.cpp
///
/// @brief Tests for TypeId.
#include "sema/TypeId.hpp"

#include <catch2/catch_test_macros.hpp>

using lucid::sema::TypeId;

TEST_CASE("TypeId is invalid by default", "[sema][type-id]")
{
    TypeId t;
    CHECK_FALSE(t.isValid());
    CHECK(t.kind == TypeId::Kind::Invalid);
}

TEST_CASE("TypeId primitive constructor", "[sema][type-id]")
{
    TypeId t = TypeId::primitive("float32");
    CHECK(t.isValid());
    CHECK(t.isPrimitive());
    CHECK_FALSE(t.isEnum());
    CHECK_FALSE(t.isHandle());
    CHECK_FALSE(t.isEvent());
    CHECK(t.isValue());
    CHECK(t.name == "float32");
}

TEST_CASE("TypeId enum constructor", "[sema][type-id]")
{
    TypeId t = TypeId::enumType("Key");
    CHECK(t.isEnum());
    CHECK(t.isValue());
    CHECK(t.name == "Key");
}

TEST_CASE("TypeId handle constructor", "[sema][type-id]")
{
    TypeId t = TypeId::handle("BodyRef");
    CHECK(t.isHandle());
    CHECK(t.isValue());
    CHECK(t.name == "BodyRef");
}

TEST_CASE("TypeId Event is not a value", "[sema][type-id]")
{
    TypeId t = TypeId::event();
    CHECK(t.isEvent());
    CHECK_FALSE(t.isValue());
    CHECK(t.name == "Event");
}

TEST_CASE("TypeId equality compares kind and name", "[sema][type-id]")
{
    CHECK(TypeId::primitive("float32") == TypeId::primitive("float32"));
    CHECK(TypeId::primitive("float32") != TypeId::primitive("float64"));
    CHECK(TypeId::enumType("Key")      != TypeId::handle("Key"));
    CHECK(TypeId::event()              == TypeId::event());
}
