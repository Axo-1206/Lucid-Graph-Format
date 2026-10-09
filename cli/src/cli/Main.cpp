/// @file cli/src/cli/Main.cpp
///
/// @brief The `lucid` entry point and subcommand dispatcher.
///
/// ─── The dispatch ─────────────────────────────────────────────────────────
/// `main` parses argv into a `CLIOptions`, then dispatches on
/// `opts.subcommand`. Each subcommand has its own `run*` function in
/// `cli/commands/`. The help and version flags are handled here, at
/// the top level, before any command runs.
///
/// ─── Why one binary ───────────────────────────────────────────────────────
/// A single `lucid` tool is easier to ship, document, and extend. A
/// new subcommand (`lucid info` for inspecting a `.lucgraph`, a
/// future `lucid lsp`) is a new `run*` and a new arm in the switch.

#include "cli/CLIOptions.hpp"
#include "cli/Version.hpp"
#include "cli/commands/Check.hpp"
#include "cli/commands/Compile.hpp"
#include "cli/commands/Format.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace
{

    constexpr const char *kProgramName = "lucid";

    void printTopLevelHelp()
    {
        std::cout << "Usage: lucid <command> [options] [file]\n"
                     "\n"
                     "A tool for the Lucid graph format.\n"
                     "\n"
                     "Commands:\n"
                     "  format    Format a .lucid file into canonical text.\n"
                     "  check     Compile a .lucid file and report diagnostics.\n"
                     "  compile   Compile a .lucid file to a .lucgraph binary.\n"
                     "\n"
                     "Options:\n"
                     "  -h, --help      Print this help and exit.\n"
                     "  -v, --version   Print the version and exit.\n"
                     "\n"
                     "Run 'lucid <command> --help' for command-specific help.\n";
    }

    void printSubcommandHelp(lucid::cli::Subcommand c)
    {
        using lucid::cli::Subcommand;
        switch (c)
        {
        case Subcommand::Format:
            std::cout << "Usage: lucid format [options] [file]\n"
                         "\n"
                         "Format a .lucid file into canonical text.\n"
                         "\n"
                         "Arguments:\n"
                         "  file                  The .lucid file to format. If omitted\n"
                         "                        or `-`, read from stdin.\n"
                         "\n"
                         "Options:\n"
                         "  -o, --output <path>   Write the formatted text to <path>.\n"
                         "                        If omitted, write to stdout.\n"
                         "  -h, --help            Print this help and exit.\n";
            return;

        case Subcommand::Check:
            std::cout << "Usage: lucid check [options] [file]\n"
                         "\n"
                         "Compile a .lucid file and report diagnostics.\n"
                         "\n"
                         "Arguments:\n"
                         "  file                  The .lucid file to check. If omitted\n"
                         "                        or `-`, read from stdin.\n"
                         "\n"
                         "Options:\n"
                         "  -h, --help            Print this help and exit.\n";
            return;

        case Subcommand::Compile:
            std::cout << "Usage: lucid compile [options] [file]\n"
                         "\n"
                         "Compile a .lucid file to a .lucgraph binary.\n"
                         "\n"
                         "Arguments:\n"
                         "  file                  The .lucid file to compile. If omitted\n"
                         "                        or `-`, read from stdin; then -o is\n"
                         "                        required.\n"
                         "\n"
                         "Options:\n"
                         "  -o, --output <path>   Write the .lucgraph to <path>. If omitted\n"
                         "                        and the input is a file, the output\n"
                         "                        path is the input path with .lucid\n"
                         "                        replaced by .lucgraph.\n"
                         "  -h, --help            Print this help and exit.\n";
            return;

        case Subcommand::None:
            printTopLevelHelp();
            return;
        }
    }

    void printVersion()
    {
        std::cout << kProgramName << ' ' << lucid::cli::kVersion << '\n';
    }

} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> args(argv, argv + argc);
    lucid::cli::CLIOptions opts = lucid::cli::CLIOptions::parse(args);

    // ─── Top-level help and version ────────────────────────────────────────
    if (opts.wantsVersion())
    {
        printVersion();
        return 0;
    }

    if (opts.wantsHelp())
    {
        // Help before a subcommand → top-level help.
        // Help after a subcommand → that subcommand's help.
        printSubcommandHelp(opts.subcommand);
        return 0;
    }

    // ─── No subcommand, no flags ──────────────────────────────────────────
    if (!opts.hasSubcommand())
    {
        if (opts.valid())
        {
            // `lucid` with no arguments: print top-level help. Not an
            // error; a user exploring the tool should see the menu.
            printTopLevelHelp();
            return 0;
        }
        // `lucid --foo` or another pre-subcommand error.
        std::cerr << kProgramName << ": " << opts.parseError << '\n';
        std::cerr << "Try '" << kProgramName << " --help' for usage.\n";
        return 2;
    }

    // ─── Parse error within a subcommand ──────────────────────────────────
    if (!opts.valid())
    {
        std::cerr << kProgramName << ' '
                  << lucid::cli::subcommandName(opts.subcommand)
                  << ": " << opts.parseError << '\n';
        std::cerr << "Try '" << kProgramName << ' '
                  << lucid::cli::subcommandName(opts.subcommand)
                  << " --help' for usage.\n";
        return 2;
    }

    // ─── Dispatch ─────────────────────────────────────────────────────────
    switch (opts.subcommand)
    {
    case lucid::cli::Subcommand::Format:
        return lucid::cli::commands::runFormat(opts);

    case lucid::cli::Subcommand::Check:
        return lucid::cli::commands::runCheck(opts);

    case lucid::cli::Subcommand::Compile:
        return lucid::cli::commands::runCompile(opts);

    case lucid::cli::Subcommand::None:
        // Unreachable; handled above.
        printTopLevelHelp();
        return 0;
    }

    return 2; // unreachable
}