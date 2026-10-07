/// @file tests/cli/test_cli_options.cpp
///
/// @brief Tests for CLIOptions::parse.

#include "cli/CLIOptions.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using lucid::cli::CLIOptions;

namespace
{

    CLIOptions parse(std::initializer_list<std::string> args)
    {
        std::vector<std::string> v{"lucid-fmt"};
        v.insert(v.end(), args.begin(), args.end());
        return CLIOptions::parse(v);
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Happy paths
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CLIOptions parses an empty argument list", "[cli-options]")
{
    const CLIOptions opts = parse({});
    CHECK(opts.valid());
    CHECK(opts.inputFile.empty());
    CHECK(opts.outputFile.empty());
    CHECK_FALSE(opts.wantsHelp());
    CHECK_FALSE(opts.wantsVersion());
}

TEST_CASE("CLIOptions parses an input file", "[cli-options]")
{
    const CLIOptions opts = parse({"file.lucid"});
    CHECK(opts.valid());
    CHECK(opts.inputFile == "file.lucid");
}

TEST_CASE("CLIOptions parses an output file", "[cli-options]")
{
    const CLIOptions opts = parse({"-o", "out.lucid", "in.lucid"});
    CHECK(opts.valid());
    CHECK(opts.inputFile == "in.lucid");
    CHECK(opts.outputFile == "out.lucid");
}

TEST_CASE("CLIOptions parses --output", "[cli-options]")
{
    const CLIOptions opts = parse({"--output", "out.lucid", "in.lucid"});
    CHECK(opts.valid());
    CHECK(opts.outputFile == "out.lucid");
}

TEST_CASE("CLIOptions parses -h and --help", "[cli-options]")
{
    CHECK(parse({"-h"}).wantsHelp());
    CHECK(parse({"--help"}).wantsHelp());
}

TEST_CASE("CLIOptions parses -v and --version", "[cli-options]")
{
    CHECK(parse({"-v"}).wantsVersion());
    CHECK(parse({"--version"}).wantsVersion());
}

TEST_CASE("CLIOptions treats '-' as stdin, not as an option", "[cli-options]")
{
    const CLIOptions opts = parse({"-"});
    CHECK(opts.valid());
    CHECK(opts.inputFile == "-");
}

// ─────────────────────────────────────────────────────────────────────────────
// Errors
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CLIOptions reports an unknown option", "[cli-options]")
{
    const CLIOptions opts = parse({"--frobnicate"});
    CHECK_FALSE(opts.valid());
    CHECK(opts.parseError.find("--frobnicate") != std::string::npos);
}

TEST_CASE("CLIOptions reports a missing argument for -o", "[cli-options]")
{
    const CLIOptions opts = parse({"-o"});
    CHECK_FALSE(opts.valid());
    CHECK(opts.parseError.find("-o") != std::string::npos);
}

TEST_CASE("CLIOptions reports multiple input files", "[cli-options]")
{
    const CLIOptions opts = parse({"a.lucid", "b.lucid"});
    CHECK_FALSE(opts.valid());
    CHECK(opts.parseError.find("multiple") != std::string::npos);
}