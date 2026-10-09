/// @file cli/src/cli/commands/Compile.cpp
///
/// @brief Implementation of the `lucid-compile` command.

#include "cli/commands/Compile.hpp"

#include "cli/Registry.hpp"
#include "serialization/Serializer.hpp"
#include "sema/Sema.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

        bool writeFileBytes(const std::string &path,
                            const std::vector<uint8_t> &bytes,
                            std::string &err)
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                err = "cannot open file for writing: " + path;
                return false;
            }
            out.write(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
            if (!out)
            {
                err = "write failed: " + path;
                return false;
            }
            return true;
        }

        // ─── Default output path ───────────────────────────────────────────

        /// Compute the default output path from the input path:
        /// `foo.lucid` → `foo.lucgraph`. If the input does not end with
        /// `.lucid`, `.lucgraph` is appended.
        std::string defaultOutputPath(std::string_view inputPath)
        {
            std::string out(inputPath);
            const std::string suffix = ".lucid";
            if (out.size() >= suffix.size() &&
                out.compare(out.size() - suffix.size(),
                            suffix.size(), suffix) == 0)
            {
                out.resize(out.size() - suffix.size());
            }
            out += ".lucgraph";
            return out;
        }

        // ─── Diagnostics ───────────────────────────────────────────────────

        void printDiagnostics(
            const std::vector<lucid::diag::Diagnostic> &diagnostics)
        {
            for (const auto &d : diagnostics)
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
        }

    } // namespace

    // ─── The command ──────────────────────────────────────────────────────────

    int runCompile(const CLIOptions &opts)
    {
        // ─── Determine input ───────────────────────────────────────────────
        const bool fromStdin =
            opts.inputFile.empty() || opts.inputFile == "-";

        // `-o` is required for stdin input. Without it, there is no
        // sensible name to write.
        if (fromStdin && opts.outputFile.empty())
        {
            std::cerr << "lucid compile: -o <path> is required when "
                         "reading from stdin\n";
            return 2;
        }

        // ─── Read source ───────────────────────────────────────────────────
        std::string source;
        std::string fileName;

        if (fromStdin)
        {
            source = readStdin();
            fileName = "<stdin>";
        }
        else
        {
            std::string err;
            if (!readFile(opts.inputFile, source, err))
            {
                std::cerr << "lucid compile: " << err << "\n";
                return 2;
            }
            fileName = opts.inputFile;
        }

        // ─── Resolve the output path ───────────────────────────────────────
        const std::string outputPath =
            opts.outputFile.empty() ? defaultOutputPath(fileName)
                                    : opts.outputFile;

        // ─── Set up the module loader ──────────────────────────────────────
        //
        // Imports are resolved relative to the input file's directory.
        // `import a.b` looks for `a/b.lucid` next to the input. This
        // matches `lucid-check`'s behavior.
        std::filesystem::path rootDir;
        if (!fromStdin)
        {
            rootDir = std::filesystem::path(fileName).parent_path();
        }
        else
        {
            rootDir = std::filesystem::current_path();
        }

        lucid::sema::CompileOptions semaOptions;
        semaOptions.loadModule =
            [rootDir](std::string_view modulePath)
            -> std::optional<std::string>
        {
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

        // ─── Compile ───────────────────────────────────────────────────────
        const lucid::sema::CompileResult result =
            lucid::sema::compile(source, fileName,
                                 lucid::cli::defaultRegistry(),
                                 semaOptions);

        printDiagnostics(result.diagnostics);

        if (!result.ok || result.graph == nullptr)
        {
            std::cerr << "lucid compile: compilation failed\n";
            return 1;
        }

        // ─── Serialize ─────────────────────────────────────────────────────
        const std::vector<uint8_t> bytes =
            lucid::serialization::serialize(*result.graph);

        // ─── Write ─────────────────────────────────────────────────────────
        std::string err;
        if (!writeFileBytes(outputPath, bytes, err))
        {
            std::cerr << "lucid compile: " << err << "\n";
            return 2;
        }

        return 0;
    }

} // namespace lucid::cli::commands