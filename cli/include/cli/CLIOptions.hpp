/// @file cli/CLIOptions.hpp
///
/// @brief The parsed command-line options for the CLI tools.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A value type holding the parsed arguments. It is populated by
/// `CLIOptions::parse`, which takes argv and returns either a filled
/// CLIOptions or a usage error.
///
/// ─── What this is not ─────────────────────────────────────────────────────
/// It is not the command's behavior. FormatCommand, CheckCommand, and
/// their peers take a CLIOptions and act on it. CLIOptions only holds
/// the data.

#pragma once

#include <string>
#include <vector>

namespace lucid::cli
{

    /// @brief The parsed command-line options.
    struct CLIOptions
    {
        /// The input file. Empty when reading from stdin.
        std::string inputFile;

        /// The output file. Empty when writing to stdout.
        std::string outputFile;

        /// When true, print help and exit.
        bool showHelp = false;

        /// When true, print version and exit.
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

        /// @brief Parse `argv` into a CLIOptions.
        ///
        /// On a well-formed argument list, returns a CLIOptions with
        /// `parseError` empty and the fields populated. On a malformed
        /// argument list, returns a CLIOptions with `parseError` set
        /// to a message.
        ///
        /// The first element of `args` is the program name and is
        /// ignored.
        static CLIOptions parse(const std::vector<std::string> &args);
    };

} // namespace lucid::cli