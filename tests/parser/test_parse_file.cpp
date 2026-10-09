/// @file tests/parser/test_parse_file.cpp
///
/// @brief End-to-end tests for parseFile.
///
/// ─── What these tests cover ───────────────────────────────────────────────
/// `parseFile` is the parser's only public entry point. These tests run
/// it on full source strings and check the resulting ModuleAST, the
/// diagnostics, and the error recovery behavior.
///
/// The tests here are the parser's integration tests. The unit tests
/// (test_parse_type, test_parse_value, test_parse_node, test_parse_decl)
/// test the individual rules/ functions; these
/// tests check that the whole pipeline — lexer, top-level loop, recovery,
/// module construction — works together.
///
/// ─── Test shape ───────────────────────────────────────────────────────────
/// Each test uses a Fixture that owns the four resources a ParserContext
/// needs, runs parseFile, and asserts on the ModuleAST and the diagnostics.

#include "parser/Parser.hpp"

#include "core/Tokens.hpp"
#include "core/ast/DeclAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <utility>
#include <vector>

using lucid::diag::DiagCode;
using lucid::diag::DiagnosticEngine;
using lucid::parser::parseFile;
using lucid::parser::ParserContext;
using lucid::parser::TokenStream;

namespace
{

    struct Fixture
    {
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag;
        TokenStream stream;

        Fixture()
            : pool(), arena(), diag(&pool),
              stream(std::vector<Token>{
                  Token{TokenType::EOF_TOKEN, InternedString{},
                        SourceLocation{1, 1}}}) {}

        ParserContext makeContext()
        {
            return ParserContext(pool, arena, diag, stream);
        }

        ModuleAST *run(std::string_view source)
        {
            ParserContext ctx = makeContext();
            return parseFile("test.lucid", source, ctx);
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Empty and trivial inputs
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseFile on empty input returns an empty module",
          "[parse-file]")
{
    Fixture f;
    ModuleAST *module = f.run("");

    REQUIRE(module != nullptr);
    CHECK(module->isEmpty());
    CHECK(module->declCount() == 0);
    CHECK_FALSE(module->hasErrors);
    CHECK(f.diag.empty());
}

TEST_CASE("parseFile on whitespace-only input returns an empty module",
          "[parse-file]")
{
    Fixture f;
    ModuleAST *module = f.run("   \n\t  \n");

    REQUIRE(module != nullptr);
    CHECK(module->isEmpty());
    CHECK_FALSE(module->hasErrors);
    CHECK(f.diag.empty());
}

TEST_CASE("parseFile on comment-only input returns an empty module",
          "[parse-file]")
{
    Fixture f;
    ModuleAST *module = f.run("-- just a comment\n");

    REQUIRE(module != nullptr);
    CHECK(module->isEmpty());
    CHECK_FALSE(module->hasErrors);
    CHECK(f.diag.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Each declaration kind, in isolation
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseFile parses a single import", "[parse-file]")
{
    Fixture f;
    ModuleAST *module = f.run("import keys");

    REQUIRE(module != nullptr);
    REQUIRE(module->declCount() == 1);
    CHECK(module->decls[0]->isa<ImportDeclAST>());
    CHECK_FALSE(module->hasErrors);
    CHECK(f.diag.empty());
}

TEST_CASE("parseFile parses a single enum", "[parse-file]")
{
    Fixture f;
    ModuleAST *module = f.run("enum Key { W, A, S, D }");

    REQUIRE(module != nullptr);
    REQUIRE(module->declCount() == 1);
    CHECK(module->decls[0]->isa<EnumDeclAST>());
    CHECK_FALSE(module->hasErrors);
}

TEST_CASE("parseFile parses a single resource", "[parse-file]")
{
    Fixture f;
    const char *source =
        "resource Player {\n"
        "    speed: float = 200.0,\n"
        "    hp: int = 100\n"
        "}\n";
    ModuleAST *module = f.run(source);

    REQUIRE(module != nullptr);
    REQUIRE(module->declCount() == 1);
    CHECK(module->decls[0]->isa<ResourceDeclAST>());
    CHECK_FALSE(module->hasErrors);
}

TEST_CASE("parseFile parses a single node", "[parse-file]")
{
    Fixture f;
    ModuleAST *module = f.run("node speed = Float32Node(200.0)");

    REQUIRE(module != nullptr);
    REQUIRE(module->declCount() == 1);
    CHECK(module->decls[0]->isa<NodeDeclAST>());
    CHECK_FALSE(module->hasErrors);
}

// ─────────────────────────────────────────────────────────────────────────────
// Multiple declarations, in order
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseFile parses multiple declarations in source order",
          "[parse-file]")
{
    Fixture f;
    const char *source =
        "import core.keys\n"
        "\n"
        "enum Direction { Left, Right }\n"
        "\n"
        "resource Player {\n"
        "    hp: int = 100\n"
        "}\n"
        "\n"
        "node speed = Float32Node(200.0)\n";
    ModuleAST *module = f.run(source);

    REQUIRE(module != nullptr);
    REQUIRE(module->declCount() == 4);
    CHECK(module->decls[0]->isa<ImportDeclAST>());
    CHECK(module->decls[1]->isa<EnumDeclAST>());
    CHECK(module->decls[2]->isa<ResourceDeclAST>());
    CHECK(module->decls[3]->isa<NodeDeclAST>());
    CHECK_FALSE(module->hasErrors);
}

TEST_CASE("parseFile preserves the source order of declarations",
          "[parse-file]")
{
    Fixture f;
    // The grammar allows any declaration order. Verify that the parser
    // emits them in source order, not grouped by kind.
    const char *source =
        "node a = Foo()\n"
        "node b = Bar()\n"
        "resource R { x: int }\n"
        "node c = Baz()\n";
    ModuleAST *module = f.run(source);

    REQUIRE(module != nullptr);
    REQUIRE(module->declCount() == 4);
    CHECK(module->decls[0]->isa<NodeDeclAST>());
    CHECK(module->decls[1]->isa<NodeDeclAST>());
    CHECK(module->decls[2]->isa<ResourceDeclAST>());
    CHECK(module->decls[3]->isa<NodeDeclAST>());
}

// ─────────────────────────────────────────────────────────────────────────────
// Attributes on declarations
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseFile attaches attributes to a resource", "[parse-file]")
{
    Fixture f;
    const char *source =
        "@export\n"
        "resource Player { hp: int = 100 }\n";
    ModuleAST *module = f.run(source);

    REQUIRE(module != nullptr);
    REQUIRE(module->declCount() == 1);
    auto *res = module->decls[0]->as<ResourceDeclAST>();
    REQUIRE(res->attributes.size() == 1);
    CHECK(f.pool.lookupView(res->attributes[0]->name) ==
          std::string_view{"export"});
}

TEST_CASE("parseFile uses the attribute's location for the declaration",
          "[parse-file]")
{
    Fixture f;
    const char *source = "@export resource Player { }\n";
    ModuleAST *module = f.run(source);

    REQUIRE(module != nullptr);
    REQUIRE(module->declCount() == 1);
    // The declaration's location is the `@`, not the `resource`.
    CHECK(module->decls[0]->loc.column() == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Error recovery
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseFile recovers from a bad declaration",
          "[parse-file]")
{
    Fixture f;
    // The first declaration is malformed (no `=` after the name). The
    // parser should report and then parse the second declaration.
    const char *source =
        "node broken\n"
        "node good = Foo()\n";
    ModuleAST *module = f.run(source);

    REQUIRE(module != nullptr);
    CHECK(module->hasErrors);
    // The malformed declaration's `node broken` starts a NodeDeclAST
    // that reports the missing `=`. The parser's recovery then finds
    // `node good` and parses it.
    // At least one declaration should be present (the recovery path may
    // produce a marked declaration for the first one).
    CHECK(module->declCount() >= 1);
}

TEST_CASE("parseFile recovers from an unknown top-level token",
          "[parse-file]")
{
    Fixture f;
    // `42` is not a declaration start. The parser should skip it and
    // parse the following `node`.
    const char *source =
        "42\n"
        "node good = Foo()\n";
    ModuleAST *module = f.run(source);

    REQUIRE(module != nullptr);
    CHECK(module->hasErrors);
    // The `42` produces an error; the parser recovers and parses the node.
    REQUIRE(module->declCount() >= 1);
    CHECK(module->decls[0]->isa<NodeDeclAST>());
}

TEST_CASE("parseFile reports a stray closing brace",
          "[parse-file]")
{
    Fixture f;
    const char *source = "}\n";
    ModuleAST *module = f.run(source);

    REQUIRE(module != nullptr);
    CHECK(module->hasErrors);
    CHECK(f.diag.hasErrors());
    // The stray `}` is reported; the loop recovers by consuming it.
    // The module is empty because there were no valid declarations.
    CHECK(module->isEmpty());
}

TEST_CASE("parseFile produces multiple diagnostics for multiple errors",
          "[parse-file]")
{
    Fixture f;
    const char *source =
        "node a\n"
        "node b\n"
        "node c\n";
    ModuleAST *module = f.run(source);

    REQUIRE(module != nullptr);
    CHECK(module->hasErrors);
    // Each malformed declaration reports at least one error.
    CHECK(f.diag.errorCount() >= 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// The module's structure
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseFile's module carries the file path", "[parse-file]")
{
    Fixture f;
    ParserContext ctx = f.makeContext();
    ModuleAST *module = parseFile("some/path.lucid", "", ctx);

    REQUIRE(module != nullptr);
    CHECK(f.pool.lookupView(module->filePath) ==
          std::string_view{"some/path.lucid"});
}

TEST_CASE("parseFile's diagnostics carry the file path", "[parse-file]")
{
    Fixture f;
    ParserContext ctx = f.makeContext();
    (void)parseFile("some/path.lucid", "node broken", ctx);

    REQUIRE_FALSE(f.diag.empty());
    for (const auto &d : f.diag.all())
    {
        CHECK(d.file.isValid());
        CHECK(f.pool.lookupView(d.file) == std::string_view{"some/path.lucid"});
    }
}

TEST_CASE("parseFile's module location is set", "[parse-file]")
{
    Fixture f;
    ModuleAST *module = f.run("");

    REQUIRE(module != nullptr);
    CHECK(module->loc.line() == 1);
    CHECK(module->loc.column() == 1);
}

TEST_CASE("parseFile reports a non-declaration token at the top level",
          "[parse-file]")
{
    Fixture f;
    ModuleAST *module = f.run("42");

    REQUIRE(module != nullptr);
    CHECK(module->hasErrors);
    CHECK(f.diag.hasErrors());
    // The `42` produces an ExpectedDeclaration diagnostic.
    bool found = false;
    for (const auto &d : f.diag.all())
    {
        if (d.code == DiagCode::Syntax_ExpectedDeclaration)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}
