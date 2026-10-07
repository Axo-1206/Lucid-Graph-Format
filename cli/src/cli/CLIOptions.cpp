/// @file cli/src/cli/CLIOptions.cpp
///
/// @brief Implementation of CLIOptions::parse.

#include "cli/CLIOptions.hpp"

namespace lucid::cli
{

    CLIOptions CLIOptions::parse(const std::vector<std::string> &args)
    {
        CLIOptions opts;

        // args[0] is the program name. Skip it.
        size_t i = 1;

        while (i < args.size())
        {
            const std::string &arg = args[i];

            // ─── Help ──────────────────────────────────────────────────────
            if (arg == "-h" || arg == "--help")
            {
                opts.showHelp = true;
                ++i;
                continue;
            }

            // ─── Version ───────────────────────────────────────────────────
            if (arg == "-v" || arg == "--version")
            {
                opts.showVersion = true;
                ++i;
                continue;
            }

            // ─── Output ────────────────────────────────────────────────────
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

            // ─── End-of-options marker ─────────────────────────────────────
            if (arg == "--")
            {
                ++i;
                break;
            }

            // ─── Any other dash-prefixed token is an error ─────────────────
            if (!arg.empty() && arg[0] == '-' && arg != "-")
            {
                opts.parseError = "unknown option: " + arg;
                return opts;
            }

            // ─── Positional: the input file ────────────────────────────────
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

        // ─── The remaining arguments are the input file ────────────────────
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
        }

        return opts;
    }

} // namespace lucid::cli