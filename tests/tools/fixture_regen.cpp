// tests/tools/fixture_regen.cpp
//
// Regenerates all fixture expected files:
//
//   1. Parser good/*.lucid → good/*.json
//      For each source, parses it and dumps the AST to JSON.
//
//   2. Formatter canonical/*.lucid → canonical/*.expected
//      For each source, parses and formats it, then writes the result.
//      Also verifies idempotence: formatting the output again must
//      produce the same output. If it does not, the tool reports the
//      non-idempotent fixture and exits non-zero without writing.
//
// Run via:
//
//   cmake --build build --target regen-fixtures
//
// This is a tool, not a test. It is not part of the default build.

#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "formatter/Formatter.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"
#include "parser/dump/JSONDumper.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using namespace lucid;

namespace
{

    // ─── I/O helpers ──────────────────────────────────────────────────────────

    std::string readFile(const fs::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    void writeFile(const fs::path &path, const std::string &text)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    }

    std::vector<fs::path> collectLucidFiles(const fs::path &dir)
    {
        std::vector<fs::path> files;
        for (const auto &entry : fs::directory_iterator(dir))
        {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".lucid")
            {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    // ─── Parser fixture regeneration ──────────────────────────────────────────

    int regenParserFixtures(const fs::path &dir)
    {
        if (!fs::exists(dir))
        {
            std::cerr << "parser fixture directory does not exist: "
                      << dir << "\n";
            return 1;
        }

        const auto lucidFiles = collectLucidFiles(dir);

        int count = 0;
        for (const auto &lucidPath : lucidFiles)
        {
            const std::string source   = readFile(lucidPath);
            const std::string fileName = lucidPath.filename().string();

            StringPool pool;
            ASTArena   arena;
            diag::DiagnosticEngine diagEngine(&pool);
            parser::TokenStream dummyStream(std::vector<Token>{
                Token{TokenType::EOF_TOKEN, InternedString{},
                      SourceLocation{1, 1}}});
            parser::ParserContext ctx(pool, arena, diagEngine, dummyStream);

            ModuleAST *module = parser::parseFile(fileName, source, ctx);
            const std::string json = parser::dump::dumpModule(module, pool);

            fs::path jsonPath = lucidPath;
            jsonPath.replace_extension(".json");
            writeFile(jsonPath, json);

            std::cout << "  wrote " << jsonPath.filename().string() << "\n";
            ++count;
        }

        std::cout << "regenerated " << count
                  << " parser fixture(s) in " << dir << "\n";
        return 0;
    }

    // ─── Formatter fixture regeneration ───────────────────────────────────────

    int regenFormatterFixtures(const fs::path &dir)
    {
        if (!fs::exists(dir))
        {
            std::cerr << "formatter fixture directory does not exist: "
                      << dir << "\n";
            return 1;
        }

        const auto lucidFiles = collectLucidFiles(dir);

        int count = 0;
        for (const auto &lucidPath : lucidFiles)
        {
            const std::string source   = readFile(lucidPath);
            const std::string fileName = lucidPath.filename().string();

            // First format pass.
            const formatter::FormatResult first =
                formatter::format(source, fileName);

            if (!first.ok)
            {
                std::cerr << "format failed for " << fileName << "\n";
                for (const auto &d : first.diagnostics)
                {
                    std::cerr << "  " << d.message << "\n";
                }
                return 1;
            }

            // Idempotence check: formatting the output must reproduce
            // the output. If it does not, the fixture is non-idempotent
            // and must not be written — the formatter has a bug.
            const formatter::FormatResult second =
                formatter::format(first.text, fileName);

            if (!second.ok)
            {
                std::cerr << "second format failed for " << fileName
                          << " (non-idempotent)\n";
                return 1;
            }

            if (first.text != second.text)
            {
                std::cerr << "non-idempotent fixture: " << fileName << "\n";
                std::cerr << "  first  format output:\n" << first.text  << "\n";
                std::cerr << "  second format output:\n" << second.text << "\n";
                return 1;
            }

            fs::path expectedPath = lucidPath;
            expectedPath.replace_extension(".expected");
            writeFile(expectedPath, first.text);

            std::cout << "  wrote " << expectedPath.filename().string() << "\n";
            ++count;
        }

        std::cout << "regenerated " << count
                  << " formatter fixture(s) in " << dir << "\n";
        return 0;
    }

} // namespace

int main()
{
#ifndef FIXTURE_DIR
    std::cerr << "FIXTURE_DIR is not defined\n";
    return 1;
#else
    const fs::path root = fs::path(FIXTURE_DIR);

    std::cout << "=== Parser fixtures ===\n";
    if (regenParserFixtures(root / "parser" / "good") != 0)
        return 1;

    std::cout << "\n=== Formatter fixtures ===\n";
    if (regenFormatterFixtures(root / "formatter" / "canonical") != 0)
        return 1;

    return 0;
#endif
}
