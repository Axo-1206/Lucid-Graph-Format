/// @file cli/src/cli/CheckMain.cpp
///
/// @brief The `lucid-check` entry point.

#include "cli/CLIOptions.hpp"
#include "cli/commands/Check.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace
{

    constexpr const char *kProgramName = "lucid-check";
    constexpr const char *kVersion = "0.1.0";

    void printHelp()
    {
        std::cout << "Usage: lucid-check [options] [file]\n"
                     "\n"
                     "Compile a .lucid file and report diagnostics.\n"
                     "\n"
                     "Arguments:\n"
                     "  file                  The .lucid file to check. If omitted\n"
                     "                        or `-`, read from stdin.\n"
                     "\n"
                     "Options:\n"
                     "  -h, --help            Print this help and exit.\n"
                     "  -v, --version         Print the version and exit.\n";
    }

    void printVersion()
    {
        std::cout << kProgramName << ' ' << kVersion << '\n';
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

    return lucid::cli::commands::runCheck(opts);
}