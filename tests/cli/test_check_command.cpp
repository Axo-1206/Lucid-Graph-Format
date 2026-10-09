/// @file tests/cli/test_check_command.cpp
///
/// @brief Tests for the lucid-check command implementation.

#include "cli/CLIOptions.hpp"
#include "cli/commands/Check.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

using lucid::cli::CLIOptions;
using lucid::cli::commands::runCheck;

namespace
{

    struct TempFile
    {
        fs::path path;

        explicit TempFile(const std::string &suffix)
            : path(fs::temp_directory_path() /
                   ("lucid_check_test_" + suffix + ".lucid"))
        {
            fs::remove(path);
        }

        ~TempFile() { fs::remove(path); }

        void write(const std::string &content) const
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << content;
        }

        std::string string() const { return path.string(); }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Success cases
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("runCheck returns 0 for a valid file", "[check-cmd]")
{
    TempFile file("valid");
    file.write("node x = Float32Node(1.5)\n");

    CLIOptions opts;
    opts.inputFile = file.string();

    CHECK(runCheck(opts) == 0);
}

TEST_CASE("runCheck returns 0 for an empty file", "[check-cmd]")
{
    TempFile file("empty");
    file.write("");

    CLIOptions opts;
    opts.inputFile = file.string();

    CHECK(runCheck(opts) == 0);
}

TEST_CASE("runCheck returns 0 for a resource-only file", "[check-cmd]")
{
    TempFile file("resource");
    file.write("resource R { x: int32 = 1 }\n");

    CLIOptions opts;
    opts.inputFile = file.string();

    CHECK(runCheck(opts) == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Failure cases
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("runCheck returns 1 for a file with parse errors", "[check-cmd]")
{
    TempFile file("parse-broken");
    file.write("node = Float32Node(1.5)\n");

    CLIOptions opts;
    opts.inputFile = file.string();

    CHECK(runCheck(opts) == 1);
}

TEST_CASE("runCheck returns 1 for a file with an unknown node type",
          "[check-cmd]")
{
    TempFile file("unknown-type");
    file.write("node x = NoSuchNode()\n");

    CLIOptions opts;
    opts.inputFile = file.string();

    CHECK(runCheck(opts) == 1);
}

TEST_CASE("runCheck returns 1 for an action node with no `on`",
          "[check-cmd]")
{
    TempFile file("action-no-on");
    file.write("node m = MoveBody(nil)\n");

    CLIOptions opts;
    opts.inputFile = file.string();

    CHECK(runCheck(opts) == 1);
}

TEST_CASE("runCheck returns 2 for a missing input file", "[check-cmd]")
{
    CLIOptions opts;
    opts.inputFile = "/definitely/not/a/real/path.lucid";

    CHECK(runCheck(opts) == 2);
}

// ─── The `-o` rejection lives in the parser ──────────────────────────────
//
// `runCheck` used to reject `-o` at the command level. As of Step 9.1,
// the rejection is in `CLIOptions::parse`, so the check happens before
// the command is ever called. A test that constructs a `CLIOptions` by
// hand and calls `runCheck` cannot exercise this path — the parser is
// not in the loop. The test below exercises the parser instead.

TEST_CASE("lucid check: -o is rejected by the parser", "[check-cmd]")
{
    using lucid::cli::CLIOptions;

    std::vector<std::string> args = {
        "lucid", "check", "-o", "/tmp/out.lucid", "in.lucid"};
    const CLIOptions opts = CLIOptions::parse(args);

    CHECK_FALSE(opts.valid());
}

// ─────────────────────────────────────────────────────────────────────────────
// Multi-module imports
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("runCheck resolves a local import", "[check-cmd]")
{
    // Create a temp directory with two files:
    //   main.lucid imports keys
    //   keys.lucid declares `Key` (exported)
    fs::path dir = fs::temp_directory_path() / "lucid_check_import";
    fs::remove_all(dir);
    fs::create_directories(dir);

    {
        std::ofstream out(dir / "main.lucid");
        out << "import keys\n"
               "resource R { k: Key = Key.W }\n";
    }
    {
        std::ofstream out(dir / "keys.lucid");
        out << "@export enum Key { W, A, S, D }\n";
    }

    CLIOptions opts;
    opts.inputFile = (dir / "main.lucid").string();

    const int result = runCheck(opts);

    fs::remove_all(dir);

    CHECK(result == 0);
}