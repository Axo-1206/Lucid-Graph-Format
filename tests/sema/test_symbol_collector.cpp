/// @file tests/sema/test_symbol_collector.cpp
///
/// @brief Tests for Pass 1's symbol collector.
///
/// ─── Test shape ───────────────────────────────────────────────────────────
/// Each test parses a small source string, runs collectSymbols on the
/// resulting module, and asserts on the symbol table's contents.
///
/// This is the first test in the codebase that combines the parser and
/// Sema. It verifies that Pass 1 sees the declarations the parser
/// produced and turns them into the right symbols.

#include "sema/SymbolTable.hpp"

#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "parser/Parser.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <utility>
#include <vector>

// The collector is internal; the test links against the same target as
// Sema and can include the internal header through the private include
// path.
#include "sema/SymbolCollector.hpp"

using lucid::diag::DiagnosticEngine;
using lucid::parser::parseFile;
using lucid::parser::ParserContext;
using lucid::parser::TokenStream;
using lucid::sema::collectSymbols;
using lucid::sema::SymbolKind;
using lucid::sema::SymbolTable;

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

        ModuleAST *parse(std::string_view source)
        {
            ParserContext ctx(pool, arena, diag, stream);
            return parseFile("test.lucid", source, ctx);
        }

        SymbolTable collect(std::string_view source)
        {
            ModuleAST *module = parse(source);
            SymbolTable table;
            collectSymbols(module, table, diag);
            return table;
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Each declaration kind
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("collectSymbols on an empty module", "[sema][symbol-collector]")
{
    Fixture f;
    SymbolTable table = f.collect("");
    CHECK(table.empty());
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("collectSymbols collects an import", "[sema][symbol-collector]")
{
    Fixture f;
    SymbolTable table = f.collect("import core.keys");

    REQUIRE(table.size() == 1);
    const auto *s = table.find(f.pool.intern("keys"));
    REQUIRE(s != nullptr);
    CHECK(s->kind == SymbolKind::Import);
}

TEST_CASE("collectSymbols collects an aliased import",
          "[sema][symbol-collector]")
{
    Fixture f;
    SymbolTable table = f.collect("import core.keys as k");

    REQUIRE(table.size() == 1);
    const auto *s = table.find(f.pool.intern("k"));
    REQUIRE(s != nullptr);
    CHECK(s->kind == SymbolKind::Import);
}

TEST_CASE("collectSymbols collects an enum", "[sema][symbol-collector]")
{
    Fixture f;
    SymbolTable table = f.collect("enum Key { W, A, S, D }");

    REQUIRE(table.size() == 1);
    const auto *s = table.find(f.pool.intern("Key"));
    REQUIRE(s != nullptr);
    CHECK(s->kind == SymbolKind::Enum);
}

TEST_CASE("collectSymbols collects a resource", "[sema][symbol-collector]")
{
    Fixture f;
    SymbolTable table = f.collect("resource Player { hp: int }");

    REQUIRE(table.size() == 1);
    const auto *s = table.find(f.pool.intern("Player"));
    REQUIRE(s != nullptr);
    CHECK(s->kind == SymbolKind::Resource);
}

TEST_CASE("collectSymbols collects a node", "[sema][symbol-collector]")
{
    Fixture f;
    SymbolTable table = f.collect("node speed = Float32Node(200.0)");

    REQUIRE(table.size() == 1);
    const auto *s = table.find(f.pool.intern("speed"));
    REQUIRE(s != nullptr);
    CHECK(s->kind == SymbolKind::Node);
}

TEST_CASE("collectSymbols collects a composite",
          "[sema][symbol-collector]")
{
    Fixture f;
    SymbolTable table = f.collect("composite Health { input { max: int } }");

    REQUIRE(table.size() == 1);
    const auto *s = table.find(f.pool.intern("Health"));
    REQUIRE(s != nullptr);
    CHECK(s->kind == SymbolKind::Composite);
}

// ─────────────────────────────────────────────────────────────────────────────
// Multiple symbols and order
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("collectSymbols preserves declaration order",
          "[sema][symbol-collector]")
{
    Fixture f;
    const char *source =
        "enum Direction { N, S, E, W }\n"
        "resource Player { hp: int }\n"
        "node speed = Float32Node(1.0)\n"
        "composite Health { input { max: int } }\n";
    SymbolTable table = f.collect(source);

    REQUIRE(table.size() == 4);
    CHECK(table.all()[0].kind == SymbolKind::Enum);
    CHECK(table.all()[1].kind == SymbolKind::Resource);
    CHECK(table.all()[2].kind == SymbolKind::Node);
    CHECK(table.all()[3].kind == SymbolKind::Composite);
}

TEST_CASE("collectSymbols reports a duplicate",
          "[sema][symbol-collector]")
{
    Fixture f;
    SymbolTable table = f.collect(
        "node a = Foo()\n"
        "node a = Bar()\n");

    CHECK(table.size() == 1);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.errorCount() == 1);
    CHECK(f.diag.all()[0].code ==
          lucid::diag::DiagCode::Name_Redeclaration);
}

TEST_CASE("collectSymbols reports multiple duplicates",
          "[sema][symbol-collector]")
{
    Fixture f;
    SymbolTable table = f.collect(
        "resource R { x: int }\n"
        "resource R { y: int }\n"
        "enum R { A }\n");

    CHECK(table.size() == 1);
    CHECK(f.diag.errorCount() == 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Declarations that do not create symbols
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("collectSymbols skips UnknownAST nodes",
          "[sema][symbol-collector]")
{
    Fixture f;
    // The `node = Foo()` is a syntax error; the parser produces an
    // UnknownAST or a malformed NodeDeclAST with no name. Pass 1
    // should not add a symbol for it.
    SymbolTable table = f.collect(
        "node = Foo()\n"
        "node good = Foo()\n");

    // The bad declaration may or may not have been parsed into a
    // NodeDeclAST with an invalid name; either way, Pass 1 should
    // produce a table with only `good`.
    const auto *good = table.find(f.pool.intern("good"));
    REQUIRE(good != nullptr);
    CHECK(good->kind == SymbolKind::Node);
}

TEST_CASE("collectSymbols does not descend into composites",
          "[sema][symbol-collector]")
{
    Fixture f;
    // The composite's internal resource and node are not top-level
    // symbols. Only the composite's name is.
    const char *source =
        "composite Health {\n"
        "  resource State { current: int = 0 }\n"
        "  node init = SetOnStart(State.current, 0)\n"
        "}\n";
    SymbolTable table = f.collect(source);

    REQUIRE(table.size() == 1);
    CHECK(table.find(f.pool.intern("Health")) != nullptr);
    CHECK(table.find(f.pool.intern("State")) == nullptr);
    CHECK(table.find(f.pool.intern("init")) == nullptr);
}
