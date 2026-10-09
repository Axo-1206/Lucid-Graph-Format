/// @file cli/commands/Check.hpp
///
/// @brief The `lucid-check` command's entry point.

#pragma once

#include "cli/CLIOptions.hpp"

namespace lucid::cli::commands
{

    /// @brief Run the `lucid-check` command.
    ///
    /// Reads the input (file or stdin), runs Sema's compile, prints
    /// diagnostics, and returns an exit code.
    ///
    /// @return The exit code: 0 on success, 1 on a compile error, 2 on
    ///         an IO or usage error.
    int runCheck(const CLIOptions &opts);

} // namespace lucid::cli::commands