/// @file tests/parser/test_parser_skeleton.cpp
///
/// @brief Tests for the Phase 2 stub parser.
///
/// ─── What these tests check ───────────────────────────────────────────────
/// The stubs do almost nothing. There are only three facts worth checking:
///
///   1. `parseFile` returns a non-null ModuleAST*, even on empty input
///      and even when every parse function is a stub.
///
///   2. The returned ModuleAST has `hasErrors == true`, because the
///      stubs report Internal_NotImplemented.
///
///   3. The DiagnosticEngine contains at least one Internal_NotImplemented
///      diagnostic after the call, tagged with the file.
///
/// Anything beyond those three is a real parsing test, and the real
/// parsing tests land in Phase 3 and Phase 4. This file will be replaced
/// or extended then; it exists now to prove the skeleton links.

#include "parser/Parser.hpp"

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

    // ─── The fixture ──────────────────────────────────────────────────────────
    //
    // parseFile takes a ParserContext by reference. The context holds
    // references to a StringPool, an ASTArena, a DiagnosticEngine, and a
    // TokenStream. The parser only ever uses the pool, arena, and diag; the
    // stream argument is a leftover from an earlier design where parseFile
    // reused the caller's stream. The current parseFile does not touch it.
    //
    // The fixture owns the four resources and constructs the context. This
    // keeps each test's body a single call to parseFile.

    struct Fixture
    {
        StringPool pool;
        ASTArena arena;
        DiagnosticEngine diag;
        TokenStream stream;

        Fixture()
            : pool(),
              arena(),
              diag(&pool),
              stream(std::vector<Token>{
                  Token{TokenType::EOF_TOKEN, InternedString{}, SourceLocation{1, 1}}}) {}

        ParserContext makeContext() { return ParserContext(pool, arena, diag, stream); }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// The three skeleton facts
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("parseFile returns a non-null ModuleAST on empty input",
          "[parser-skeleton]")
{
    Fixture f;
    ParserContext ctx = f.makeContext();

    ModuleAST *module = parseFile("test.lucid", "", ctx);

    REQUIRE(module != nullptr);
}

TEST_CASE("parseFile on empty input sets hasErrors",
          "[parser-skeleton]")
{
    Fixture f;
    ParserContext ctx = f.makeContext();

    ModuleAST *module = parseFile("test.lucid", "", ctx);

    REQUIRE(module != nullptr);
    CHECK(module->hasErrors);
}

TEST_CASE("parseFile reports NotImplemented and tags the file",
          "[parser-skeleton]")
{
    Fixture f;
    ParserContext ctx = f.makeContext();

    (void)parseFile("test.lucid", "", ctx);

    REQUIRE_FALSE(f.diag.empty());
    CHECK(f.diag.hasErrors());

    // The stub reports exactly one diagnostic. The real implementation
    // will report many; this assertion will be relaxed in Phase 3.
    CHECK(f.diag.totalCount() == 1);

    const auto &diagnostics = f.diag.all();
    CHECK(diagnostics[0].code == DiagCode::Internal_NotImplemented);
    CHECK(diagnostics[0].isError());

    // The file identity should be the interned "test.lucid". The guard
    // sets it and restores it, so the engine's currentFile is back to
    // its previous (invalid) value after the call, but the diagnostic
    // itself carries the file.
    CHECK(diagnostics[0].file.isValid());
}

TEST_CASE("parseFile's returned module carries the interned path",
          "[parser-skeleton]")
{
    Fixture f;
    ParserContext ctx = f.makeContext();

    ModuleAST *module = parseFile("test.lucid", "", ctx);

    REQUIRE(module != nullptr);
    CHECK(module->filePath.isValid());
    CHECK(f.pool.lookupView(module->filePath) == std::string_view{"test.lucid"});
}

TEST_CASE("parseFile's returned module is empty",
          "[parser-skeleton]")
{
    Fixture f;
    ParserContext ctx = f.makeContext();

    ModuleAST *module = parseFile("test.lucid", "", ctx);

    REQUIRE(module != nullptr);
    CHECK(module->isEmpty());
    CHECK(module->declCount() == 0);
}