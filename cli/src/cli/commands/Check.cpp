/// @file cli/src/cli/commands/Check.cpp
///
/// @brief Implementation of the `lucid-check` command.

#include "cli/commands/Check.hpp"
#include "cli/Registry.hpp"

#include "sema/Sema.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace lucid::cli::commands
{

    namespace
    {

        // ─── File IO ───────────────────────────────────────────────────────

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

    } // namespace

    // ─── The command ──────────────────────────────────────────────────────────

    int runCheck(const CLIOptions &opts)
    {
        if (!opts.outputFile.empty())
        {
            std::cerr << "lucid-check: -o is not supported\n";
            return 2;
        }

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
                std::cerr << "lucid-check: " << err << "\n";
                return 2;
            }
            fileName = opts.inputFile;
        }

        // ─── Compile ───────────────────────────────────────────────────────
        //
        // `compile` handles imports via the CompileOptions::loadModule
        // callback. For `lucid-check`, imports are resolved relative to
        // the checked file's directory: an import `a.b` looks for
        // `a/b.lucid` next to the checked file.
        //
        // The loader is deliberately simple. A more sophisticated tool
        // would search a package root; this tool checks one file at a
        // time and resolves imports locally.
        std::filesystem::path rootDir;
        if (fileName != "<stdin>")
        {
            rootDir = std::filesystem::path(fileName).parent_path();
        }
        else
        {
            rootDir = std::filesystem::current_path();
        }

        lucid::sema::CompileOptions semaOptions;
        semaOptions.loadModule =
            [rootDir](std::string_view modulePath) -> std::optional<std::string>
        {
            // `core.keys` → `core/keys.lucid`
            std::string relative;
            relative.reserve(modulePath.size() + 6);
            for (char c : modulePath)
            {
                relative += (c == '.') ? '/' : c;
            }
            relative += ".lucid";

            const std::filesystem::path path = rootDir / relative;
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return std::nullopt;

            std::ostringstream ss;
            ss << in.rdbuf();
            return ss.str();
        };

        const lucid::sema::CompileResult result =
            lucid::sema::compile(source, fileName,
                                 lucid::cli::defaultRegistry(), semaOptions);

        // ─── Report diagnostics ────────────────────────────────────────────
        for (const auto &d : result.diagnostics)
        {
            std::cerr << lucid::diag::severityName(d.severity) << ": "
                      << d.message;
            if (d.location.isKnown())
            {
                std::cerr << " at " << d.location.line()
                          << ':' << d.location.column();
            }
            std::cerr << '\n';
        }

        // ─── Exit code ─────────────────────────────────────────────────────
        if (!result.ok)
        {
            return 1;
        }
        return 0;
    }

} // namespace lucid::cli::commands