/// @file cli/src/cli/CompileMain.cpp
///
/// @brief The `lucid-compile` entry point.

#include "cli/CLIOptions.hpp"
#include "cli/Version.hpp"
#include "cli/commands/Compile.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace
{

    constexpr const char *kProgramName = "lucid-compile";

    void printHelp()
    {
        std::cout << "Usage: lucid-compile [options] [file]\n"
                     "\n"
                     "Compile a .lucid file to a .lucgraph binary.\n"
                     "\n"
                     "Arguments:\n"
                     "  file                  The .lucid file to compile. If omitted\n"
                     "                        or `-`, read from stdin; then -o is\n"
                     "                        required.\n"
                     "\n"
                     "Options:\n"
                     "  -o, --output <path>   Write the .lucgraph to <path>. If omitted\n"
                     "                        and the input is a file, the output\n"
                     "                        path is the input path with .lucid\n"
                     "                        replaced by .lucgraph.\n"
                     "  -h, --help            Print this help and exit.\n"
                     "  -v, --version         Print the version and exit.\n";
    }

    void printVersion()
    {
        std::cout << kProgramName << ' ' << lucid::cli::kVersion << '\n';
    }

} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> args(argv, argv + argc);
    lucid::cli::CLIOptions opts = lucid::cli::CLIOptions::parse(args);

    if (opts.wantsHelp())
    {
        printHelp();
        return 0;
    }

    if (opts.wantsVersion())
    {
        printVersion();
        return 0;
    }

    if (!opts.valid())
    {
        std::cerr << kProgramName << ": " << opts.parseError << '\n';
        std::cerr << "Try '" << kProgramName << " --help' for usage.\n";
        return 2;
    }

    return lucid::cli::commands::runCompile(opts);
}