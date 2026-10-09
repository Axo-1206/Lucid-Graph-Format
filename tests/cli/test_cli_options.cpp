/// @file tests/cli/test_cli_options.cpp
///
/// @brief Unit tests for CLIOptions::parse with subcommands.

#include "cli/CLIOptions.hpp"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string>
#include <vector>

using namespace lucid::cli;

namespace
{

    CLIOptions parse(std::initializer_list<const char *> args)
    {
        std::vector<std::string> v(args.begin(), args.end());
        return CLIOptions::parse(v);
    }

} // namespace

// ─── Top level ────────────────────────────────────────────────────────────

TEST_CASE("lucid: no arguments shows help, not an error",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid"});
    CHECK(opts.valid());
    CHECK_FALSE(opts.hasSubcommand());
    CHECK_FALSE(opts.wantsHelp());
    CHECK_FALSE(opts.wantsVersion());
}

TEST_CASE("lucid: top-level --help",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "--help"});
    CHECK(opts.valid());
    CHECK(opts.wantsHelp());
    CHECK_FALSE(opts.hasSubcommand());
}

TEST_CASE("lucid: top-level --version",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "--version"});
    CHECK(opts.valid());
    CHECK(opts.wantsVersion());
}

TEST_CASE("lucid: unknown subcommand is an error",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "frobnicate"});
    CHECK_FALSE(opts.valid());
    CHECK_FALSE(opts.parseError.empty());
}

TEST_CASE("lucid: a flag before the subcommand is an error",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "--output", "out", "format"});
    CHECK_FALSE(opts.valid());
}

// ─── Subcommand: format ───────────────────────────────────────────────────

TEST_CASE("lucid format: plain file",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "format", "in.lucid"});
    CHECK(opts.valid());
    CHECK(opts.subcommand == Subcommand::Format);
    CHECK(opts.inputFile == "in.lucid");
    CHECK(opts.outputFile.empty());
}

TEST_CASE("lucid format: -o",
          "[cli][options]")
{
    const CLIOptions opts =
        parse({"lucid", "format", "-o", "out.lucid", "in.lucid"});
    CHECK(opts.valid());
    CHECK(opts.subcommand == Subcommand::Format);
    CHECK(opts.inputFile == "in.lucid");
    CHECK(opts.outputFile == "out.lucid");
}

TEST_CASE("lucid format: --help",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "format", "--help"});
    CHECK(opts.valid());
    CHECK(opts.wantsHelp());
    CHECK(opts.subcommand == Subcommand::Format);
}

TEST_CASE("lucid format: --version after subcommand is an error",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "format", "--version"});
    CHECK_FALSE(opts.valid());
}

// ─── Subcommand: check ────────────────────────────────────────────────────

TEST_CASE("lucid check: plain file",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "check", "in.lucid"});
    CHECK(opts.valid());
    CHECK(opts.subcommand == Subcommand::Check);
    CHECK(opts.inputFile == "in.lucid");
}

TEST_CASE("lucid check: -o is rejected",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "check", "-o", "out", "in.lucid"});
    CHECK_FALSE(opts.valid());
}

// ─── Subcommand: compile ──────────────────────────────────────────────────

TEST_CASE("lucid compile: plain file",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "compile", "in.lucid"});
    CHECK(opts.valid());
    CHECK(opts.subcommand == Subcommand::Compile);
    CHECK(opts.inputFile == "in.lucid");
}

TEST_CASE("lucid compile: -o",
          "[cli][options]")
{
    const CLIOptions opts =
        parse({"lucid", "compile", "-o", "out.lucgraph", "in.lucid"});
    CHECK(opts.valid());
    CHECK(opts.subcommand == Subcommand::Compile);
    CHECK(opts.inputFile == "in.lucid");
    CHECK(opts.outputFile == "out.lucgraph");
}

// ─── Common errors ────────────────────────────────────────────────────────

TEST_CASE("lucid <cmd>: multiple input files is an error",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "format", "a.lucid", "b.lucid"});
    CHECK_FALSE(opts.valid());
}

TEST_CASE("lucid <cmd>: unknown option is an error",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "format", "--bogus", "in.lucid"});
    CHECK_FALSE(opts.valid());
}

TEST_CASE("lucid <cmd>: -- alone ends option parsing",
          "[cli][options]")
{
    const CLIOptions opts = parse({"lucid", "format", "--", "in.lucid"});
    CHECK(opts.valid());
    CHECK(opts.inputFile == "in.lucid");
}