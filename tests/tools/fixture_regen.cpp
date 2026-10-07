// tests/tools/fixture_regen.cpp
//
// Regenerates the parser fixture expected files. For each
// `FIXTURE_DIR/parser/good/*.lucid`, parses the file, dumps its AST
// to JSON, and writes the result to the sibling `*.json`.
//
// This is a tool, not a test. It is built by the `regen-fixtures`
// custom target and is not part of the default build.

#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"
#include "parser/dump/JSONDumper.hpp"

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

    int regenDirectory(const fs::path &dir)
    {
        if (!fs::exists(dir))
        {
            std::cerr << "fixture directory does not exist: " << dir << "\n";
            return 1;
        }

        // Collect the .lucid files.
        std::vector<fs::path> lucidFiles;
        for (const auto &entry : fs::directory_iterator(dir))
        {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".lucid")
            {
                lucidFiles.push_back(entry.path());
            }
        }
        std::sort(lucidFiles.begin(), lucidFiles.end());

        int count = 0;
        for (const auto &lucidPath : lucidFiles)
        {
            const std::string source = readFile(lucidPath);
            const std::string fileName = lucidPath.filename().string();

            StringPool pool;
            ASTArena arena;
            diag::DiagnosticEngine diag(&pool);
            parser::TokenStream stream(std::vector<Token>{
                Token{TokenType::EOF_TOKEN, InternedString{}, SourceLocation{1, 1}}});
            parser::ParserContext ctx(pool, arena, diag, stream);

            ModuleAST *module = parser::parseFile(fileName, source, ctx);
            const std::string json = parser::dump::dumpModule(module, pool);

            fs::path jsonPath = lucidPath;
            jsonPath.replace_extension(".json");
            writeFile(jsonPath, json);

            std::cout << "wrote " << jsonPath.filename().string() << "\n";
            ++count;
        }

        std::cout << "regenerated " << count << " fixture(s) in "
                  << dir << "\n";
        return 0;
    }

} // namespace

int main()
{
#ifndef FIXTURE_DIR
    std::cerr << "FIXTURE_DIR is not defined\n";
    return 1;
#else
    const fs::path root = fs::path(FIXTURE_DIR) / "parser";
    if (regenDirectory(root / "good") != 0)
        return 1;
    return 0;
#endif
}