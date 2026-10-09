/// @file cli/src/cli/Main.cpp
///
/// @brief The `lucid-fmt` entry point.

#include "cli/CLIOptions.hpp"
#include "cli/commands/Format.hpp"
#include "cli/Version.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace
{

    constexpr const char *kProgramName = "lucid-fmt";

    void printHelp()
    {
        std::cout << "Usage: lucid-fmt [options] [file]\n"
                     "\n"
                     "Format a .lucid file into canonical text.\n"
                     "\n"
                     "Arguments:\n"
                     "  file                  The .lucid file to format. If omitted\n"
                     "                        or `-`, read from stdin.\n"
                     "\n"
                     "Options:\n"
                     "  -o, --output <path>   Write the formatted text to <path>.\n"
                     "                        If omitted, write to stdout.\n"
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

    return lucid::cli::commands::runFormat(opts);
}