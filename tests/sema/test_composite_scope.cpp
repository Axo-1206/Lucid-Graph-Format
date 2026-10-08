/// @file tests/sema/test_composite_scope.cpp
///
/// @brief Tests for CompositeScope.

#include "sema/CompositeScope.hpp"

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

#include "sema/SymbolCollector.hpp"

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

        ModuleAST *parse(std::string_view source)
        {
            ParserContext ctx(pool, arena, diag, stream);
            return parseFile("test.lucid", source, ctx);
        }

        struct Run
        {
            SymbolTable moduleSymbols;
            CompositeScope scope;

            /// Convenience: lookup with the fixture's module table.
            const Symbol *find(InternedString name) const noexcept
            {
                return scope.find(name, moduleSymbols);
            }
        };

        Run runFirstComposite(std::string_view source)
        {
            ModuleAST *module = parse(source);

            Run r;
            collectSymbols(module, r.moduleSymbols, diag);

            CompositeDeclAST *composite = nullptr;
            for (DeclAST *decl : module->decls)
            {
                if (decl != nullptr && decl->kind == ASTKind::CompositeDecl)
                {
                    composite = decl->as<CompositeDeclAST>();
                    break;
                }
            }

            r.scope = buildCompositeScope(composite, diag);
            return r;
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Local names
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CompositeScope contains composite inputs",
          "[sema][composite-scope]")
{
    Fixture f;
    auto r = f.runFirstComposite(
        "composite Health {\n"
        "  input { max: int32 }\n"
        "  node init = SetOnStart(max, max)\n"
        "}\n");

    CHECK_FALSE(f.diag.hasErrors());
    const auto *sym = r.find(f.pool.intern("max"));
    REQUIRE(sym != nullptr);
    CHECK(sym->kind == SymbolKind::CompositeInput);
}

TEST_CASE("CompositeScope contains composite resources",
          "[sema][composite-scope]")
{
    Fixture f;
    auto r = f.runFirstComposite(
        "composite Health {\n"
        "  resource State { hp: int32 = 0 }\n"
        "}\n");

    CHECK_FALSE(f.diag.hasErrors());
    const auto *sym = r.find(f.pool.intern("State"));
    REQUIRE(sym != nullptr);
    CHECK(sym->kind == SymbolKind::Resource);
}

TEST_CASE("CompositeScope contains composite nodes",
          "[sema][composite-scope]")
{
    Fixture f;
    auto r = f.runFirstComposite(
        "composite Health {\n"
        "  node init = Foo()\n"
        "}\n");

    CHECK_FALSE(f.diag.hasErrors());
    const auto *sym = r.find(f.pool.intern("init"));
    REQUIRE(sym != nullptr);
    CHECK(sym->kind == SymbolKind::Node);
}

// ─────────────────────────────────────────────────────────────────────────────
// Module fallback
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CompositeScope falls back to the module's symbols",
          "[sema][composite-scope]")
{
    Fixture f;
    auto r = f.runFirstComposite(
        "enum Direction { N, S, E, W }\n"
        "composite Health {\n"
        "  input { max: int32 }\n"
        "}\n");

    const auto *sym = r.find(f.pool.intern("Direction"));
    REQUIRE(sym != nullptr);
    CHECK(sym->kind == SymbolKind::Enum);
}

TEST_CASE("CompositeScope prefers local over module symbols",
          "[sema][composite-scope]")
{
    Fixture f;
    auto r = f.runFirstComposite(
        "resource max { hp: int32 = 0 }\n"
        "composite Health {\n"
        "  input { max: int32 }\n"
        "}\n");

    const auto *sym = r.find(f.pool.intern("max"));
    REQUIRE(sym != nullptr);
    CHECK(sym->kind == SymbolKind::CompositeInput);
}

TEST_CASE("CompositeScope returns nullptr for a missing name",
          "[sema][composite-scope]")
{
    Fixture f;
    auto r = f.runFirstComposite(
        "composite Health {\n"
        "  input { max: int32 }\n"
        "}\n");

    CHECK(r.find(f.pool.intern("nope")) == nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Composite-internal imports
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CompositeScope contains composite-internal imports",
          "[sema][composite-scope]")
{
    Fixture f;
    auto r = f.runFirstComposite(
        "composite Health {\n"
        "  import core.keys as k\n"
        "}\n");

    CHECK_FALSE(f.diag.hasErrors());
    const auto *sym = r.find(f.pool.intern("k"));
    REQUIRE(sym != nullptr);
    CHECK(sym->kind == SymbolKind::Import);
}

// ─────────────────────────────────────────────────────────────────────────────
// Duplicate detection within the composite
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CompositeScope reports a duplicate local name",
          "[sema][composite-scope]")
{
    Fixture f;
    auto r = f.runFirstComposite(
        "composite Health {\n"
        "  input { x: int32 }\n"
        "  resource x { hp: int32 = 0 }\n"
        "}\n");

    CHECK(f.diag.hasErrors());
    bool found = false;
    for (const auto &d : f.diag.all())
    {
        if (d.code == lucid::diag::DiagCode::Name_Redeclaration)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}