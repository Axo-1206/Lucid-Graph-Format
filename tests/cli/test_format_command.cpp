/// @file tests/cli/test_format_command.cpp
///
/// @brief Tests for the lucid-fmt command implementation.

#include "cli/CLIOptions.hpp"
#include "cli/commands/Format.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

using lucid::cli::CLIOptions;
using lucid::cli::commands::runFormat;

namespace
{

    struct TempFile
    {
        fs::path path;

        explicit TempFile(const std::string &suffix)
            : path(fs::temp_directory_path() /
                   ("lucid_fmt_test_" + suffix + ".lucid"))
        {
            fs::remove(path);
        }

        ~TempFile() { fs::remove(path); }

        void write(const std::string &content) const
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << content;
        }

        std::string read() const
        {
            std::ifstream in(path, std::ios::binary);
            std::ostringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// -o
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("runFormat -o writes the formatted text", "[format-cmd]")
{
    TempFile in("write-in");
    TempFile out("write-out");
    in.write("node a=Foo()\n");

    CLIOptions opts;
    opts.inputFile = in.path.string();
    opts.outputFile = out.path.string();

    CHECK(runFormat(opts) == 0);
    CHECK(out.read() == "node a = Foo()\n");
}

TEST_CASE("runFormat -o returns 2 when the output cannot be written",
          "[format-cmd]")
{
    TempFile in("write-fail-in");
    in.write("node a = Foo()\n");

    CLIOptions opts;
    opts.inputFile = in.path.string();
    opts.outputFile =
        (fs::temp_directory_path() / "definitely" / "not" / "a" /
         "real" / "path" / "out.lucid")
            .string();

    CHECK(runFormat(opts) == 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Reading errors
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("runFormat returns 2 for a missing input file", "[format-cmd]")
{
    CLIOptions opts;
    opts.inputFile =
        (fs::temp_directory_path() / "definitely" / "not" / "a" /
         "real" / "path" / "in.lucid")
            .string();
    CHECK(runFormat(opts) == 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse errors
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("runFormat returns 1 for input with parse errors", "[format-cmd]")
{
    TempFile in("parse-broken");
    in.write("node = Foo()\n");

    CLIOptions opts;
    opts.inputFile = in.path.string();

    CHECK(runFormat(opts) == 1);
}