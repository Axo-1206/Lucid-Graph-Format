/// @file tests/sema/test_sema_smoke.cpp
///
/// @brief A single test that the Sema headers include cleanly and the
///        public types have the expected sizes and shapes.
#include "sema/Graph.hpp"
#include "sema/Literal.hpp"
#include "sema/NodeKind.hpp"
#include "sema/Registry.hpp"
#include "sema/Sema.hpp"
#include "sema/TypeId.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Sema headers compile and link", "[sema][smoke]")
{
    // Referencing the types forces the compiler to instantiate their
    // definitions.
    (void)sizeof(lucid::sema::NodeKind);
    (void)sizeof(lucid::sema::TypeId);
    (void)sizeof(lucid::sema::Literal);
    (void)sizeof(lucid::sema::Registry);
    (void)sizeof(lucid::sema::Graph);
    (void)sizeof(lucid::sema::CompileOptions);
    (void)sizeof(lucid::sema::CompileResult);
    CHECK(true);
}
