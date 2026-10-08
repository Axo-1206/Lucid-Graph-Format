/// @file tests/sema/test_resolver.cpp
///
/// @brief Tests for Pass 2: name resolution.

#include "sema/ResolutionMap.hpp"
#include "sema/SymbolTable.hpp"

#include "core/ast/DeclAST.hpp"
#include "core/ast/ValueAST.hpp"
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

#include "sema/Resolver.hpp"
#include "sema/SymbolCollector.hpp"

using lucid::diag::DiagnosticEngine;
using lucid::parser::parseFile;
using lucid::parser::ParserContext;
using lucid::parser::TokenStream;
using lucid::sema::collectSymbols;
using lucid::sema::ResolutionMap;
using lucid::sema::resolveNames;
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

        struct Run
        {
            ModuleAST *module;
            SymbolTable symbols;
            ResolutionMap resolutions;
        };

        Run run(std::string_view source)
        {
            ParserContext ctx(pool, arena, diag, stream);
            ModuleAST *module = parseFile("test.lucid", source, ctx);

            Run r;
            r.module = module;
            collectSymbols(module, r.symbols, diag);
            resolveNames(module, r.symbols, r.resolutions, diag);
            return r;
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Identifier values
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("resolver records a node reference",
          "[sema][resolver]")
{
    Fixture f;
    auto r = f.run(
        "node body = BodyNode(player)\n"
        "node move = MoveBody(body)\n");

    // The identifier `body` inside `MoveBody(body)` should resolve to
    // the NodeDeclAST of `body`.
    const auto *bodySymbol = r.symbols.find(f.pool.intern("body"));
    REQUIRE(bodySymbol != nullptr);

    // Walk the declarations to find the `body` identifier value inside
    // the second node's expression.
    REQUIRE(r.module->declCount() == 2);
    auto *move = r.module->decls[1]->as<NodeDeclAST>();
    REQUIRE(move != nullptr);
    REQUIRE(move->expr != nullptr);
    REQUIRE(move->expr->args.size() == 1);
    auto *arg = move->expr->args[0];
    REQUIRE(arg != nullptr);
    REQUIRE(arg->isa<IdentifierValueAST>());

    const auto *resolved = r.resolutions.lookup(arg);
    CHECK(resolved == bodySymbol->decl);
}

TEST_CASE("resolver reports an undefined identifier",
          "[sema][resolver]")
{
    Fixture f;
    auto r = f.run("node move = MoveBody(unknown)\n");

    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code ==
          lucid::diag::DiagCode::Name_UndefinedNode);
}

TEST_CASE("resolver records a resource reference",
          "[sema][resolver]")
{
    Fixture f;
    auto r = f.run(
        "resource State { hp: int }\n"
        "node move = BodyNode(State)\n");

    const auto *stateSymbol = r.symbols.find(f.pool.intern("State"));
    REQUIRE(stateSymbol != nullptr);

    auto *move = r.module->decls[1]->as<NodeDeclAST>();
    auto *arg = move->expr->args[0];
    REQUIRE(arg->isa<IdentifierValueAST>());

    CHECK(r.resolutions.lookup(arg) == stateSymbol->decl);
}

// ─────────────────────────────────────────────────────────────────────────────
// Field access values
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("resolver records the object of a field access",
          "[sema][resolver]")
{
    Fixture f;
    auto r = f.run(
        "resource State { hp: int }\n"
        "node move = BodyNode(State.hp)\n");

    const auto *stateSymbol = r.symbols.find(f.pool.intern("State"));
    REQUIRE(stateSymbol != nullptr);

    auto *move = r.module->decls[1]->as<NodeDeclAST>();
    auto *arg = move->expr->args[0];
    REQUIRE(arg->isa<FieldAccessValueAST>());

    // The field access resolves to the object's declaration (`State`).
    // The field itself (`hp`) is resolved in a later pass.
    CHECK(r.resolutions.lookup(arg) == stateSymbol->decl);
}

TEST_CASE("resolver reports an undefined field access object",
          "[sema][resolver]")
{
    Fixture f;
    auto r = f.run("node move = BodyNode(Unknown.field)\n");

    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code ==
          lucid::diag::DiagCode::Name_UndefinedNode);
}

// ─────────────────────────────────────────────────────────────────────────────
// Triggers
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("resolver records a valid trigger", "[sema][resolver]")
{
    Fixture f;
    auto r = f.run(
        "node hit = OnCollision(body, \"hazard\")\n"
        "node play = PlaySound(\"hit.wav\") on hit\n");

    const auto *hitSymbol = r.symbols.find(f.pool.intern("hit"));
    REQUIRE(hitSymbol != nullptr);

    // The resolution is recorded against the NodeDeclAST of `play`
    // (the node with the `on` clause), since triggers are names without
    // their own AST node.
    auto *play = r.module->decls[1]->as<NodeDeclAST>();
    REQUIRE(play != nullptr);
    REQUIRE(play->hasTriggers());

    CHECK(r.resolutions.lookup(play) == hitSymbol->decl);
}

TEST_CASE("resolver reports an undefined trigger", "[sema][resolver]")
{
    Fixture f;
    auto r = f.run("node play = PlaySound(\"hit.wav\") on unknown\n");

    CHECK(f.diag.hasErrors());
    CHECK(f.diag.all().back().code ==
          lucid::diag::DiagCode::Name_UndefinedTrigger);
}

// ─────────────────────────────────────────────────────────────────────────────
// Type references
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("resolver resolves a type reference to an enum",
          "[sema][resolver]")
{
    Fixture f;
    auto r = f.run(
        "enum Key { W, A }\n"
        "resource Input { left: Key }\n");

    const auto *keySymbol = r.symbols.find(f.pool.intern("Key"));
    REQUIRE(keySymbol != nullptr);

    auto *input = r.module->decls[1]->as<ResourceDeclAST>();
    REQUIRE(input != nullptr);
    REQUIRE(input->fields.size() == 1);
    auto *field = input->fields[0];
    REQUIRE(field->type != nullptr);

    CHECK(r.resolutions.lookup(field->type) == keySymbol->decl);
}

TEST_CASE("resolver defers a primitive type reference",
          "[sema][resolver]")
{
    Fixture f;
    auto r = f.run("resource R { x: float32 }\n");

    auto *res = r.module->decls[0]->as<ResourceDeclAST>();
    auto *field = res->fields[0];

    // `float32` is not a module symbol; it is a registry type. The
    // resolver records a deferred (null) resolution.
    CHECK(r.resolutions.contains(field->type));
    CHECK(r.resolutions.lookup(field->type) == nullptr);

    // No diagnostic: Step 7.4 checks the registry.
    CHECK_FALSE(f.diag.hasErrors());
}

// ─────────────────────────────────────────────────────────────────────────────
// Clean modules have no diagnostics
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("resolver on a clean module reports no errors",
          "[sema][resolver]")
{
    Fixture f;
    auto r = f.run(
        "enum Direction { N, S, E, W }\n"
        "resource Player { speed: float32 = 200.0 }\n"
        "node body = BodyNode(Player)\n"
        "node move = MoveBody(body) on on_update\n"
        "node on_update = EveryFrame()\n");

    // The `on_update` trigger is defined after `move`, so the resolver
    // must find it (name resolution is order-independent).
    CHECK_FALSE(f.diag.hasErrors());
}
