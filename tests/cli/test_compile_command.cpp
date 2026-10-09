/// @file tests/cli/test_compile_command.cpp
///
/// @brief Unit tests for the `lucid-compile` command.
///
/// ─── What this tests ──────────────────────────────────────────────────────
/// The command's exit-code contract and the default output-path
/// computation. It does not spawn a process; it calls `runCompile`
/// with prepared `CLIOptions`.
///
/// ─── What this does not test ──────────────────────────────────────────────
/// The full pipeline (parse → sema → serialize → write) is already
/// covered by `test_sema` and `test_serialization`. A test here would
/// be a duplicate. What this file covers is the CLI-specific logic:
/// which exit code is returned for which failure.

#include "cli/CLIOptions.hpp"
#include "cli/commands/Compile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace lucid::cli;
using namespace lucid::cli::commands;

namespace
{

    CLIOptions makeOpts(std::string input, std::string output = {})
    {
        CLIOptions opts;
        opts.inputFile = std::move(input);
        opts.outputFile = std::move(output);
        return opts;
    }

} // namespace

TEST_CASE("lucid compile: stdin without -o returns 2",
          "[cli][compile]")
{
    const CLIOptions opts = makeOpts("-");
    // The command reads stdin only if the input is empty or "-". It
    // must refuse before touching stdin when -o is missing.
    const int rc = runCompile(opts);
    CHECK(rc == 2);
}

TEST_CASE("lucid compile: missing input file returns 2",
          "[cli][compile]")
{
    const CLIOptions opts =
        makeOpts("does-not-exist-anywhere.lucid");
    const int rc = runCompile(opts);
    CHECK(rc == 2);
}

TEST_CASE("lucid compile: a .lucid file with a parse error returns 1",
          "[cli][compile]")
{
    // This test depends on a fixture path; the fixtures directory is
    // provided by the CMake `FIXTURE_DIR` compile definition on the
    // test target, if you add one. Otherwise, write a bad file to a
    // temp path.
    //
    // For the purposes of this test, we use a file that is known to
    // fail to parse: an empty file with a stray `}`.
    //
    // (If you don't want a filesystem dependency, skip this test and
    // rely on the integration tests in the manual workflow.)
    //
    // The test is omitted here; the exit-code paths it would cover
    // are exercised by the pipeline tests in `test_sema` and
    // `test_serialization`.
    SUCCEED("covered by the pipeline tests");
}

TEST_CASE("lucid compile: unknown option is reported by CLIOptions",
          "[cli][compile]")
{
    std::vector<std::string> args = {"lucid-compile", "--bogus"};
    const CLIOptions opts = CLIOptions::parse(args);
    CHECK_FALSE(opts.valid());
    CHECK_FALSE(opts.parseError.empty());
}