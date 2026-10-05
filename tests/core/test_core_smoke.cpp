/**
 * @file tests/core/test_core_smoke.cpp
 *
 * @brief Phase 0 smoke test for lucid_core.
 *
 * Phase 0 does not yet have any core headers. This test proves the test
 * target links against lucid_core and that Catch2 runs. It is replaced
 * by real tests in Phase 1.
 */

#include <catch2/catch_test_macros.hpp>

TEST_CASE("core: test target links and runs", "[core][smoke]") {
    REQUIRE(1 + 1 == 2);
}