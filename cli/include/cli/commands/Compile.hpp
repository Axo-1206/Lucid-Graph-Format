/// @file cli/commands/Compile.hpp
///
/// @brief The `lucid-compile` command's entry point.
///
/// ─── What `lucid-compile` does ────────────────────────────────────────────
/// Reads a `.lucid` file, compiles it to a `Graph`, serializes the
/// graph to `.lucgraph` bytes, and writes the bytes to a file.
///
/// On success, the output file's contents are the output of
/// `serialization::serialize`. On any error — an IO failure, a parse
/// error, a Sema error, a serialization failure — the output file is
/// not written, and diagnostics are printed to stderr.
///
/// ─── The output path ──────────────────────────────────────────────────────
/// If `-o <path>` is given, the output goes there. Otherwise the
/// output path is the input path with `.lucid` replaced by
/// `.lucgraph`. If the input is stdin (`-` or omitted), `-o` is
/// required.

#pragma once

#include "cli/CLIOptions.hpp"

namespace lucid::cli::commands
{

    /// @brief Run the `lucid-compile` command.
    ///
    /// @return The exit code: 0 on success, 1 on a compile error, 2 on
    ///         an IO or usage error.
    int runCompile(const CLIOptions &opts);

} // namespace lucid::cli::commands