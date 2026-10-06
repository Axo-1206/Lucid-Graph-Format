/// @file tests/parser/test_parser_context.cpp
///
/// @brief Tests for ParserContext and ScopedDiagnosticFile.

#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

#include "core/Tokens.hpp"
#include "core/SourceLocation.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/InternedString.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <vector>

namespace
{

    using lucid::parser::ParserContext;
    using lucid::parser::ScopedDiagnosticFile;
    using lucid::parser::TokenStream;

    /// Build a stream with only the EOF token. Good enough for context tests
    /// that do not exercise parsing.
    TokenStream makeEmptyStream()
    {
        std::vector<Token> toks;
        toks.push_back(Token{TokenType::EOF_TOKEN, InternedString{}, SourceLocation{}});
        return TokenStream{std::move(toks)};
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// ParserContext: construction and field access
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ParserContext: holds references to its resources",
          "[parser][context]")
{
    StringPool pool;
    ASTArena arena;
    lucid::diag::DiagnosticEngine diag;
    TokenStream stream = makeEmptyStream();

    ParserContext ctx{pool, arena, diag, stream};

    REQUIRE(&ctx.pool == &pool);
    REQUIRE(&ctx.arena == &arena);
    REQUIRE(&ctx.diag == &diag);
    REQUIRE(&ctx.stream == &stream);
}

TEST_CASE("ParserContext: is not copyable",
          "[parser][context]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<ParserContext>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<ParserContext>);
}

TEST_CASE("ParserContext: is not movable",
          "[parser][context]")
{
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<ParserContext>);
    STATIC_REQUIRE_FALSE(std::is_move_assignable_v<ParserContext>);
}

// ─────────────────────────────────────────────────────────────────────────────
// ParserContext: canContinue
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ParserContext: canContinue is true when there are no errors",
          "[parser][context]")
{
    StringPool pool;
    ASTArena arena;
    lucid::diag::DiagnosticEngine diag;
    TokenStream stream = makeEmptyStream();

    ParserContext ctx{pool, arena, diag, stream};

    REQUIRE(ctx.canContinue());
}

TEST_CASE("ParserContext: canContinue is false when errors exceed the cap",
          "[parser][context]")
{
    StringPool pool;
    ASTArena arena;
    lucid::diag::DiagnosticEngine diag;
    TokenStream stream = makeEmptyStream();

    ParserContext ctx{pool, arena, diag, stream};

    // Report 100 errors, which is the default cap.
    for (int i = 0; i < 100; ++i)
    {
        diag.errorAt(lucid::diag::DiagCode::Lex_UnknownCharacter,
                     SourceLocation{1, 1}, "e");
    }

    REQUIRE_FALSE(ctx.canContinue());
    REQUIRE(ctx.diag.errorCount() == 100);
}

// ─────────────────────────────────────────────────────────────────────────────
// ScopedDiagnosticFile
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ScopedDiagnosticFile: sets the engine's current file",
          "[parser][context][scoped]")
{
    StringPool pool;
    ASTArena arena;
    lucid::diag::DiagnosticEngine diag;
    TokenStream stream = makeEmptyStream();

    ParserContext ctx{pool, arena, diag, stream};

    InternedString a = pool.intern("a.lucid");

    // Before the guard, the current file is invalid.
    REQUIRE_FALSE(diag.currentFile().isValid());

    {
        ScopedDiagnosticFile guard{ctx, a};
        REQUIRE(diag.currentFile() == a);
        REQUIRE(pool.lookupView(diag.currentFile()) == "a.lucid");
    }

    // After the guard, the previous value is restored.
    REQUIRE_FALSE(diag.currentFile().isValid());
}

TEST_CASE("ScopedDiagnosticFile: restores the previous file, not a default",
          "[parser][context][scoped]")
{
    StringPool pool;
    ASTArena arena;
    lucid::diag::DiagnosticEngine diag;
    TokenStream stream = makeEmptyStream();

    ParserContext ctx{pool, arena, diag, stream};

    InternedString outer = pool.intern("outer.lucid");
    InternedString inner = pool.intern("inner.lucid");

    diag.setCurrentFile(outer);

    {
        ScopedDiagnosticFile guard{ctx, inner};
        REQUIRE(diag.currentFile() == inner);
    }

    // The guard restored `outer`, not an invalid default.
    REQUIRE(diag.currentFile() == outer);
    REQUIRE(pool.lookupView(diag.currentFile()) == "outer.lucid");
}

TEST_CASE("ScopedDiagnosticFile: guards nest",
          "[parser][context][scoped]")
{
    StringPool pool;
    ASTArena arena;
    lucid::diag::DiagnosticEngine diag;
    TokenStream stream = makeEmptyStream();

    ParserContext ctx{pool, arena, diag, stream};

    InternedString a = pool.intern("a.lucid");
    InternedString b = pool.intern("b.lucid");
    InternedString c = pool.intern("c.lucid");

    diag.setCurrentFile(a);

    {
        ScopedDiagnosticFile outer{ctx, b};
        REQUIRE(diag.currentFile() == b);

        {
            ScopedDiagnosticFile inner{ctx, c};
            REQUIRE(diag.currentFile() == c);
        }

        // Inner guard restored `b`.
        REQUIRE(diag.currentFile() == b);
    }

    // Outer guard restored `a`.
    REQUIRE(diag.currentFile() == a);
}

TEST_CASE("ScopedDiagnosticFile: a diagnostic raised inside the guard carries the file",
          "[parser][context][scoped]")
{
    StringPool pool;
    ASTArena arena;
    lucid::diag::DiagnosticEngine diag;
    TokenStream stream = makeEmptyStream();

    ParserContext ctx{pool, arena, diag, stream};

    InternedString file = pool.intern("test.lucid");

    {
        ScopedDiagnosticFile guard{ctx, file};
        diag.errorAt(lucid::diag::DiagCode::Lex_UnknownCharacter,
                     SourceLocation{1, 1}, "test");
    }

    REQUIRE(diag.totalCount() == 1);
    REQUIRE(diag.all()[0].file == file);
}

TEST_CASE("ScopedDiagnosticFile: a diagnostic raised outside any guard has no file",
          "[parser][context][scoped]")
{
    StringPool pool;
    ASTArena arena;
    lucid::diag::DiagnosticEngine diag;
    TokenStream stream = makeEmptyStream();

    ParserContext ctx{pool, arena, diag, stream};

    diag.errorAt(lucid::diag::DiagCode::Lex_UnknownCharacter,
                 SourceLocation{1, 1}, "test");

    REQUIRE(diag.totalCount() == 1);
    REQUIRE_FALSE(diag.all()[0].file.isValid());
}

TEST_CASE("ScopedDiagnosticFile: is not copyable or movable",
          "[parser][context][scoped]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<ScopedDiagnosticFile>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<ScopedDiagnosticFile>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<ScopedDiagnosticFile>);
    STATIC_REQUIRE_FALSE(std::is_move_assignable_v<ScopedDiagnosticFile>);
}