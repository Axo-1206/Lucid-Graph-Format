/// @file cli/CLIOptions.hpp
///
/// @brief The parsed command-line options for the `lucid` tool.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A value type holding the parsed arguments. It is populated by
/// `CLIOptions::parse`, which takes argv and returns either a filled
/// CLIOptions or a usage error.
///
/// ─── The two-layer parse ──────────────────────────────────────────────────
/// The command line has a subcommand followed by its options:
///
///     lucid <command> [options] [file]
///
/// `parse` reads the subcommand first (the first non-flag argument),
/// then parses the remaining arguments the same way `lucid-fmt` and
/// its siblings did before consolidation. The `subcommand` field
/// records what was chosen; the fields below it carry the options.
///
/// ─── What this is not ─────────────────────────────────────────────────────
/// It is not the command's behavior. `runFormat`, `runCheck`, and
/// `runCompile` take a `CLIOptions` and act on it. `CLIOptions` only
/// holds the data.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lucid::cli
{

    /// @brief The subcommand the user asked for.
    enum class Subcommand : uint8_t
    {
        None, // no subcommand given; top-level help is shown
        Format,
        Check,
        Compile,
    };

    /// @brief The name of a subcommand, for diagnostics and help.
    inline const char *subcommandName(Subcommand c) noexcept
    {
        switch (c)
        {
        case Subcommand::None:
            return "(none)";
        case Subcommand::Format:
            return "format";
        case Subcommand::Check:
            return "check";
        case Subcommand::Compile:
            return "compile";
        }
        return "(unknown)";
    }

    /// @brief The parsed command-line options.
    struct CLIOptions
    {
        /// The chosen subcommand.
        Subcommand subcommand = Subcommand::None;

        /// The input file. Empty when reading from stdin.
        std::string inputFile;

        /// The output file. Empty when writing to stdout, or when the
        /// subcommand computes a default path (compile).
        std::string outputFile;

        /// When true, print help and exit. At the top level, this means
        /// top-level help. After a subcommand, it means that
        /// subcommand's help.
        bool showHelp = false;

        /// When true, print version and exit. Only meaningful at the
        /// top level.
        bool showVersion = false;

        /// Non-empty if parsing failed. The string is a human-readable
        /// error message suitable for printing to stderr.
        std::string parseError;

        /// True if the options are valid and the command should run.
        bool valid() const noexcept { return parseError.empty(); }

        /// True if the caller should print help and exit.
        bool wantsHelp() const noexcept { return showHelp; }

        /// True if the caller should print the version and exit.
        bool wantsVersion() const noexcept { return showVersion; }

        /// True if the user asked for a subcommand.
        bool hasSubcommand() const noexcept
        {
            return subcommand != Subcommand::None;
        }

        /// @brief Parse `argv` into a CLIOptions.
        ///
        /// The first element of `args` is the program name and is
        /// ignored.
        ///
        /// Grammar:
        ///
        ///   args ::= [ '--help' | '--version' ]
        ///          | subcommand { option | positional }
        ///
        /// A top-level `--help` or `--version` takes effect
        /// immediately; no subcommand is required. Otherwise the
        /// first non-flag argument is the subcommand. An unknown
        /// subcommand is a parse error.
        static CLIOptions parse(const std::vector<std::string> &args);
    };

} // namespace lucid::cli