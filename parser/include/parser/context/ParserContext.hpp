/// @file parser/context/ParserContext.hpp
///
/// @brief Per-session parser state.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// The parser is called once per source file and produces one ModuleAST.
/// It does not walk imports, does not resolve module paths, and does not
/// depend on the filesystem. Those are the CLI's jobs. Because the parser
/// does not recurse across files, this context is small: references to the
/// string pool, the AST arena, and the diagnostic engine, plus the token
/// stream for the current file.
///
/// ─── What it is not ───────────────────────────────────────────────────────
/// It is not a session. The resources it references are owned by the
/// session (or by whatever constructed the context), and outlive the
/// parse. A ParserContext borrows them; it does not extend their lifetime.
///
/// It is not per-file in the sense of "one context per file". One
/// ParserContext is constructed per parse, and every parse function in
/// that parse uses the same one.
///
/// It is not a place to stash data for a later pass. A field that is
/// written by the parser and read by Sema does not belong here; it
/// belongs on the AST node the parser produced.

#pragma once

#include "core/SourceLocation.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/InternedString.hpp"
#include "core/memory/StringPool.hpp"
#include "parser/context/TokenStream.hpp"

namespace lucid::parser
{

    /// @brief The parser's view of one parse.
    ///
    /// Constructed once per parse and passed by reference to every parse
    /// function. Holds references to the resources the parser needs; the
    /// parser reaches them as plain fields.
    struct ParserContext
    {
        /// Canonical string storage for the session.
        StringPool &pool;

        /// Bump-allocated storage for the session's AST.
        ASTArena &arena;

        /// Collected diagnostics for the session.
        lucid::diag::DiagnosticEngine &diag;

        /// The current file's tokens.
        TokenStream &stream;

        // ─── Construction ──────────────────────────────────────────────────

        ParserContext(StringPool &p,
                      ASTArena &a,
                      lucid::diag::DiagnosticEngine &d,
                      TokenStream &s)
            : pool(p), arena(a), diag(d), stream(s)
        {
        }

        // Non-copyable, non-movable. The parser holds a reference to it, and
        // the reference must remain valid for the whole parse. Making the
        // type movable would allow a copy to be made and then destroyed,
        // leaving the parser with a dangling reference.
        ParserContext(const ParserContext &) = delete;
        ParserContext &operator=(const ParserContext &) = delete;
        ParserContext(ParserContext &&) = delete;
        ParserContext &operator=(ParserContext &&) = delete;

        /// True if the parser should keep going. The error cap is the
        /// diagnostic engine's; this wrapper exists so call sites read
        /// `ctx.canContinue()` rather than reaching through to the engine.
        bool canContinue() const
        {
            return diag.canContinue();
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // ScopedDiagnosticFile
    // ─────────────────────────────────────────────────────────────────────────────

    /// @brief Tags every diagnostic raised while active with a file identity.
    ///
    /// The diagnostic engine is one per session, but a session parses many
    /// files. A diagnostic raised during a parse has to carry the file it
    /// came from, or a session that parses three files produces three files'
    /// worth of "line 12, column 5" with no way to tell them apart.
    ///
    /// This guard sets the engine's current file on construction and restores
    /// the previous value on destruction. The "previous" matters: an LSP
    /// analysis runs on a background file while the user is looking at
    /// another, and the engine's current file has to return to whatever it
    /// was after the analysis completes.
    struct ScopedDiagnosticFile
    {
        ScopedDiagnosticFile(ParserContext &ctx, InternedString file)
            : ctx_(ctx), saved_(ctx_.diag.currentFile())
        {
            ctx_.diag.setCurrentFile(file);
        }

        ~ScopedDiagnosticFile()
        {
            ctx_.diag.setCurrentFile(saved_);
        }

        ScopedDiagnosticFile(const ScopedDiagnosticFile &) = delete;
        ScopedDiagnosticFile &operator=(const ScopedDiagnosticFile &) = delete;
        ScopedDiagnosticFile(ScopedDiagnosticFile &&) = delete;
        ScopedDiagnosticFile &operator=(ScopedDiagnosticFile &&) = delete;

    private:
        ParserContext &ctx_;
        InternedString saved_;
    };

} // namespace lucid::parser