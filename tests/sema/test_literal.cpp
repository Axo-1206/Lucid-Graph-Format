/// @file tests/sema/test_literal.cpp
///
/// @brief Tests for Literal.
#include "sema/Literal.hpp"

#include <catch2/catch_test_macros.hpp>

using lucid::sema::Literal;
using lucid::sema::literalKindName;

TEST_CASE("Literal is nil by default", "[sema][literal]")
{
    Literal lit;
    CHECK(lit.kind == Literal::Kind::Nil);
    CHECK(lit.isNil());
    CHECK_FALSE(lit.isInteger());
    CHECK_FALSE(lit.isFloat());
    CHECK_FALSE(lit.isString());
}

TEST_CASE("Literal bool constructor", "[sema][literal]")
{
    Literal lit{true};
    CHECK(lit.kind == Literal::Kind::Bool);
    CHECK(lit.b == true);
}

TEST_CASE("Literal char constructor", "[sema][literal]")
{
    Literal lit{'x'};
    CHECK(lit.kind == Literal::Kind::Char);
    CHECK(lit.c == 'x');
}

TEST_CASE("Literal integer kinds are recognized", "[sema][literal]")
{
    Literal lit;

    lit.kind = Literal::Kind::Int32;
    CHECK(lit.isInteger());
    CHECK_FALSE(lit.isFloat());

    lit.kind = Literal::Kind::UInt64;
    CHECK(lit.isInteger());

    lit.kind = Literal::Kind::Float32;
    CHECK(lit.isFloat());
    CHECK_FALSE(lit.isInteger());
}

TEST_CASE("Literal string carries offset and length", "[sema][literal]")
{
    Literal lit = Literal::makeString(100, 5);
    CHECK(lit.kind == Literal::Kind::String);
    CHECK(lit.isString());
    CHECK(lit.string.offset == 100);
    CHECK(lit.string.length == 5);
}

TEST_CASE("literalKindName returns stable names", "[sema][literal]")
{
    CHECK(std::string_view(literalKindName(Literal::Kind::Nil))     == "Nil");
    CHECK(std::string_view(literalKindName(Literal::Kind::Int32))   == "Int32");
    CHECK(std::string_view(literalKindName(Literal::Kind::Float64)) == "Float64");
}
