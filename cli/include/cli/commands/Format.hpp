/// @file cli/commands/Format.hpp
///
/// @brief The `lucid-fmt` command's entry point.

#pragma once

#include "cli/CLIOptions.hpp"

namespace lucid::cli::commands
{

    /// @brief Run the `lucid-fmt` command.
    ///
    /// Reads the input (file or stdin), formats it, and writes the
    /// result (file or stdout).
    ///
    /// @return The exit code: 0 on success, 1 on a parse error in the
    ///         input, 2 on an IO or usage error.
    int runFormat(const CLIOptions &opts);

} // namespace lucid::cli::commands