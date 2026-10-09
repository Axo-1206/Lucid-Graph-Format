/// @file tests/sema/test_import_resolver.cpp
///
/// @brief Tests for the import resolver.

#include "sema/Sema.hpp"

#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sema/ImportResolver.hpp"

using lucid::diag::DiagnosticEngine;
using lucid::parser::parseFile;
using lucid::parser::ParserContext;
using lucid::parser::TokenStream;
using namespace lucid::sema;

namespace
{

    struct Fixture
    {
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag{&pool};
        TokenStream stream;

        Fixture()
            : pool(), arena(),
              stream(std::vector<Token>{
                  Token{TokenType::EOF_TOKEN, InternedString{},
                        SourceLocation{1, 1}}})
        {
        }

        /// Parse the root module from `source` with the given
        /// `filename`, using a callback that returns sources from a
        /// map.
        ModuleSet load(std::string_view source,
                       std::string_view filename,
                       std::unordered_map<std::string, std::string> modules)
        {
            ParserContext ctx(pool, arena, diag, stream);
            ModuleAST *root = parseFile(filename, source, ctx);

            CompileOptions options;
            options.loadModule = [modules = std::move(modules)](
                                     std::string_view path)
                -> std::optional<std::string>
            {
                auto it = modules.find(std::string(path));
                if (it == modules.end())
                    return std::nullopt;
                return it->second;
            };

            return resolveImports(root, filename, options,
                                  pool, arena, diag);
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// No imports
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("import resolver on a module with no imports",
          "[sema][import-resolver]")
{
    Fixture f;
    ModuleSet set = f.load("", "main.lucid", {});
    REQUIRE(set.modules.size() == 1);
    CHECK(set.modules[0]->filePath == f.pool.intern("main.lucid"));
    CHECK_FALSE(f.diag.hasErrors());
}

// ─────────────────────────────────────────────────────────────────────────────
// One import
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("import resolver loads one module",
          "[sema][import-resolver]")
{
    Fixture f;
    ModuleSet set = f.load(
        "import core.keys\n",
        "main.lucid",
        {{"core.keys", "@export enum Key { W, A }\n"}});

    CHECK_FALSE(f.diag.hasErrors());
    REQUIRE(set.modules.size() == 2);
    // Dependency order: imported first, root last.
    CHECK(set.modules[0]->filePath == f.pool.intern("core.keys"));
    CHECK(set.modules[1]->filePath == f.pool.intern("main.lucid"));
}

TEST_CASE("import resolver caches a module imported twice",
          "[sema][import-resolver]")
{
    Fixture f;
    ModuleSet set = f.load(
        "import core.keys\n"
        "import core.keys\n",
        "main.lucid",
        {{"core.keys", ""}});

    CHECK_FALSE(f.diag.hasErrors());
    // Only one load of `core.keys`, even though two imports.
    REQUIRE(set.modules.size() == 2);
}

TEST_CASE("import resolver loads transitive imports",
          "[sema][import-resolver]")
{
    Fixture f;
    ModuleSet set = f.load(
        "import a\n",
        "main.lucid",
        {
            {"a", "import b\n"},
            {"b", ""},
        });

    CHECK_FALSE(f.diag.hasErrors());
    REQUIRE(set.modules.size() == 3);
    // Dependency order: b, then a, then main.
    CHECK(set.modules[0]->filePath == f.pool.intern("b"));
    CHECK(set.modules[1]->filePath == f.pool.intern("a"));
    CHECK(set.modules[2]->filePath == f.pool.intern("main.lucid"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Errors
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("import resolver reports a missing module",
          "[sema][import-resolver]")
{
    Fixture f;
    (void)f.load(
        "import missing\n",
        "main.lucid",
        {});

    CHECK(f.diag.hasErrors());
    bool found = false;
    for (const auto &d : f.diag.all())
    {
        if (d.code == lucid::diag::DiagCode::Import_ModuleNotFound)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("import resolver reports a circular import",
          "[sema][import-resolver]")
{
    Fixture f;
    (void)f.load(
        "import a\n",
        "main.lucid",
        {
            {"a", "import b\n"},
            {"b", "import a\n"},
        });

    CHECK(f.diag.hasErrors());
    bool found = false;
    for (const auto &d : f.diag.all())
    {
        if (d.code == lucid::diag::DiagCode::Import_Circular)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}