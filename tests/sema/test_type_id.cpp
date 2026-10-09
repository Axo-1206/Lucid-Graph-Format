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

TEST_CASE("TypeId owns its name", "[sema][type-id]")
{
    const TypeId t = [] {
        const std::string name = "temporary-type";
        return TypeId::primitive(name);
    }();

    CHECK(t.name == "temporary-type");
}

TEST_CASE("TypeId equality compares kind and name", "[sema][type-id]")
{
    CHECK(TypeId::primitive("float32") == TypeId::primitive("float32"));
    CHECK(TypeId::primitive("float32") != TypeId::primitive("float64"));
    CHECK(TypeId::enumType("Key") != TypeId::handle("Key"));
}

TEST_CASE("Every valid TypeId kind is a value type", "[sema][type-id]")
{
    // Every kind TypeId can have except Invalid is a value type.
    // There is no fourth kind; the Event type was removed.
    CHECK(TypeId::primitive("bool").isValue());
    CHECK(TypeId::enumType("Direction").isValue());
    CHECK(TypeId::handle("TextureRef").isValue());
    CHECK_FALSE(TypeId{}.isValue());
}