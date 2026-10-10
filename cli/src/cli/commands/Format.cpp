/// @file cli/src/cli/commands/Format.cpp
///
/// @brief Implementation of the `lucid format` subcommand.
///
/// Invoked as `lucid format [options] [file]`. See `Main.cpp` for the
/// dispatcher and the top-level help text.

#include "cli/commands/Format.hpp"

#include "formatter/Formatter.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace lucid::cli::commands
{

    namespace
    {

        bool readFile(const std::string &path, std::string &out, std::string &err)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                err = "cannot open file: " + path;
                return false;
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            out = ss.str();
            return true;
        }

        std::string readStdin()
        {
            std::ostringstream ss;
            ss << std::cin.rdbuf();
            return ss.str();
        }

        bool writeFile(const std::string &path, const std::string &text,
                       std::string &err)
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                err = "cannot open file for writing: " + path;
                return false;
            }
            out << text;
            if (!out)
            {
                err = "write failed: " + path;
                return false;
            }
            return true;
        }

    } // namespace

    int runFormat(const CLIOptions &opts)
    {
        // ─── Read input ────────────────────────────────────────────────────
        std::string source;
        std::string fileName;

        if (opts.inputFile.empty() || opts.inputFile == "-")
        {
            source = readStdin();
            fileName = "<stdin>";
        }
        else
        {
            std::string err;
            if (!readFile(opts.inputFile, source, err))
            {
                std::cerr << "lucid format: " << err << "\n";
                return 2;
            }
            fileName = opts.inputFile;
        }

        // ─── Format ────────────────────────────────────────────────────────
        const lucid::formatter::FormatResult result =
            lucid::formatter::format(source, fileName);

        if (!result.ok)
        {
            std::cerr << "lucid format: cannot format " << fileName
                      << ": parse errors\n";
            for (const auto &d : result.diagnostics)
            {
                std::cerr << "  "
                          << lucid::diag::severityName(d.severity) << ": "
                          << d.message;
                if (d.location.isKnown())
                {
                    std::cerr << " at " << d.location.line()
                              << ':' << d.location.column();
                }
                std::cerr << "\n";
            }
            return 1;
        }

        // ─── Write output ──────────────────────────────────────────────────
        if (opts.outputFile.empty())
        {
            std::cout << result.text;
            return 0;
        }

        std::string err;
        if (!writeFile(opts.outputFile, result.text, err))
        {
            std::cerr << "lucid format: " << err << "\n";
            return 2;
        }
        return 0;
    }

} // namespace lucid::cli::commands