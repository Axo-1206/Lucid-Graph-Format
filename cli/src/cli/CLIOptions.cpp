/// @file cli/src/cli/CLIOptions.cpp
///
/// @brief Implementation of CLIOptions::parse.

#include "cli/CLIOptions.hpp"

namespace lucid::cli
{

    namespace
    {

        /// Map a subcommand name to its enum. Returns false if `name`
        /// is not a recognized subcommand.
        bool parseSubcommand(const std::string &name, Subcommand &out)
        {
            if (name == "format")
            {
                out = Subcommand::Format;
                return true;
            }
            if (name == "check")
            {
                out = Subcommand::Check;
                return true;
            }
            if (name == "compile")
            {
                out = Subcommand::Compile;
                return true;
            }
            return false;
        }

        /// True if `s` starts with `-` and is not the lone `-` (which
        /// means "read from stdin").
        bool isFlag(const std::string &s)
        {
            return !s.empty() && s[0] == '-' && s != "-";
        }

    } // namespace

    CLIOptions CLIOptions::parse(const std::vector<std::string> &args)
    {
        CLIOptions opts;

        // args[0] is the program name. Skip it.
        size_t i = 1;

        // ─── Pass 1: find the subcommand and top-level flags ──────────────
        //
        // Walk the arguments until a subcommand is found. Top-level
        // `--help` and `--version` take effect immediately; they do
        // not require a subcommand.

        while (i < args.size())
        {
            const std::string &arg = args[i];

            if (arg == "--help" || arg == "-h")
            {
                opts.showHelp = true;
                return opts;
            }
            if (arg == "--version" || arg == "-v")
            {
                opts.showVersion = true;
                return opts;
            }

            // A flag with no subcommand is an error: there is nothing
            // to apply it to.
            if (isFlag(arg))
            {
                opts.parseError = "unknown option before subcommand: " + arg;
                return opts;
            }

            // First non-flag argument is the subcommand.
            if (!parseSubcommand(arg, opts.subcommand))
            {
                opts.parseError = "unknown subcommand: '" + arg + "'";
                return opts;
            }
            ++i;
            break;
        }

        // No subcommand given.
        if (opts.subcommand == Subcommand::None)
        {
            // A completely empty command line is not an error; the
            // caller shows top-level help. But `lucid --foo` is an
            // error, and it was caught above.
            return opts;
        }

        // ─── Pass 2: parse the subcommand's options ──────────────────────

        while (i < args.size())
        {
            const std::string &arg = args[i];

            // Per-subcommand help.
            if (arg == "--help" || arg == "-h")
            {
                opts.showHelp = true;
                return opts;
            }

            // Version is only meaningful at the top level. After a
            // subcommand, treat it as unknown so the user notices the
            // misplacement.
            if (arg == "--version" || arg == "-v")
            {
                opts.parseError =
                    "--version is only valid before the subcommand";
                return opts;
            }

            // Output path.
            if (arg == "-o" || arg == "--output")
            {
                if (i + 1 >= args.size())
                {
                    opts.parseError = "missing argument for " + arg;
                    return opts;
                }
                opts.outputFile = args[i + 1];
                i += 2;
                continue;
            }

            // End-of-options marker: stop parsing options. The
            // remaining tokens are positional and are picked up
            // below.
            if (arg == "--")
            {
                ++i;
                break;
            }

            // Any other dash-prefixed token is an error.
            if (isFlag(arg))
            {
                opts.parseError = "unknown option: " + arg;
                return opts;
            }

            // Positional: the input file.
            if (!opts.inputFile.empty())
            {
                opts.parseError =
                    "multiple input files given: '" + opts.inputFile +
                    "' and '" + arg + "'";
                return opts;
            }
            opts.inputFile = arg;
            ++i;
        }

        // ─── Remaining positional after `--` ──────────────────────────────
        //
        // If the loop above exited at `--`, the remaining tokens are
        // positional. This block handles the first one. A second
        // positional is an error, matching the loop's behavior.

        if (i < args.size())
        {
            if (!opts.inputFile.empty())
            {
                opts.parseError =
                    "multiple input files given: '" + opts.inputFile +
                    "' and '" + args[i] + "'";
                return opts;
            }
            opts.inputFile = args[i];
            ++i;

            if (i < args.size())
            {
                opts.parseError =
                    "multiple input files given: '" + opts.inputFile +
                    "' and '" + args[i] + "'";
                return opts;
            }
        }

        // ─── Subcommand-specific validation ──────────────────────────────
        //
        // `-o` is meaningful for `format` and `compile`; `check` does
        // not produce output, so `-o` is an error there. This mirrors
        // the previous behavior of `lucid-check` refusing `-o`.

        if (opts.subcommand == Subcommand::Check && !opts.outputFile.empty())
        {
            opts.parseError = "check does not accept -o";
            return opts;
        }

        return opts;
    }

} // namespace lucid::cli