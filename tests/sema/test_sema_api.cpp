/// @file tests/sema/test_sema_api.cpp
///
/// @brief Smoke tests for Sema's public API. The implementations land
///        in later steps; these verify the types compile and the
///        defaults are correct.
#include "sema/Sema.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lucid::sema;

TEST_CASE("CompileOptions has no loader by default", "[sema][api]")
{
    CompileOptions opts;
    CHECK_FALSE(opts.loadModule);
}

TEST_CASE("CompileResult is unsuccessful by default", "[sema][api]")
{
    CompileResult result;
    CHECK_FALSE(result.ok);
    CHECK(result.graph == nullptr);
    CHECK(result.diagnostics.empty());
}

TEST_CASE("computeRegistryFingerprint is declared", "[sema][api]")
{
    // The function exists. Its result on an empty registry is
    // implementation-defined but stable; a later test will pin its
    // value once the algorithm is implemented. The stub returns 0.
    Registry empty;
    (void)computeRegistryFingerprint(empty);
    CHECK(true);
}
