/// @file parser/src/parser/Parser.cpp
///
/// @brief The parser's entry point and top-level dispatcher.
///
/// ─── parseDecl ────────────────────────────────────────────────────────────
/// Reads the attribute list, checks that the current token can begin a
/// declaration, dispatches via `parseDeclByKeyword` (ParseDeclInternal.hpp),
/// and attaches the attributes to the returned declaration.
///
/// Returns `nullptr` in two cases:
///
///   1. The current token cannot begin a declaration (no `@` and no
///      declaration keyword). This is the SKIP path: the caller's loop
///      runs the synchronizer.
///
///   2. An attribute list was read, but the token after it is not a
///      declaration keyword. Report `Syntax_ExpectedDeclaration` and
///      return `nullptr`.
///
/// ─── parseFile ────────────────────────────────────────────────────────────
/// Interns the path, tags diagnostics with the file, lexes the source
/// into a fresh TokenStream, runs the top-level loop, and builds the
/// ModuleAST.
///
/// The loop:
///   1. Checks `canStartTopDecl` for the current token.
///   2. If it can, calls `parseDecl`. If the result is non-null, appends
///      it to the module's declaration list.
///   3. If the token cannot begin a declaration, reports
///      `Syntax_ExpectedDeclaration` once.
///   4. If `parseDecl` returned null or step 3 fired, runs the top-level
///      stop-set synchronizer to skip to the next plausible declaration
///      start.
///   5. If the synchronizer reports ForeignCloser, reports
///      `Syntax_UnexpectedToken`, consumes the offending `}`, and
///      continues.
///   6. If the synchronizer reports ReachedEnd, exits the loop.
///
/// The loop checks `ctx.canContinue()` to respect the error cap. A file
/// with hundreds of errors stops producing diagnostics once the cap is
/// hit; the module is returned with whatever was parsed.
///
/// ─── The top-level stop set ───────────────────────────────────────────────
/// The stop set is a lambda local to `parseFile`. It stops on any token
/// that can begin a top-level declaration at brace depth 0. It does not
/// stop on a stray `}`; the scanner treats that as a foreign closer and
/// the loop exits.

#include "parser/Parser.hpp"
#include "parser/rules/ParseDeclInternal.hpp"

#include "core/diagnostics/DiagCode.hpp"
#include "parser/lexer/Lexer.hpp"
#include "parser/support/ErrorRecovery.hpp"
#include "parser/support/GrammarPositions.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    // =============================================================================
    // parseDecl — the top-level dispatcher
    // =============================================================================

    DeclAST *parseDecl(TokenStream &stream, ParserContext &ctx)
    {
        // ─── Fast rejection ────────────────────────────────────────────────
        // If the current token cannot begin a declaration and it is not
        // an attribute, there is nothing to parse. Return nullptr without
        // consuming anything; the caller's loop recovers.
        if (!stream.check(TokenType::AT_SIGN) &&
            !canStartTopDecl(stream.peekType()))
        {
            return nullptr;
        }

        // ─── The attribute list ────────────────────────────────────────────
        const SourceLocation startLoc = stream.currentLoc();
        ArenaSpan<AttributeAST *> attrs = parseAttributeList(stream, ctx);

        // ─── The declaration keyword ───────────────────────────────────────
        // After the attribute list, the next token must begin a declaration.
        if (!canStartTopDecl(stream.peekType()) ||
            stream.check(TokenType::AT_SIGN))
        {
            // An `@` here means two `@` in a row — the attribute list
            // parser should have consumed both. Reaching here is a
            // caller bug, but report it as a user-facing error for safety.
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedDeclaration,
                             stream.currentLoc(),
                             "expected a declaration after the attribute list");
            return nullptr;
        }

        // ─── The declaration ───────────────────────────────────────────────
        DeclAST *decl = parseDeclByKeyword(stream, ctx);
        if (!decl)
        {
            // parseDeclByKeyword reported the error. Return nullptr; the
            // caller's loop recovers.
            return nullptr;
        }

        // ─── Attach the attributes ─────────────────────────────────────────
        // The specific parsers set `attributes` to an empty span (the
        // DeclAST constructor default). Replace it if we read any.
        if (!attrs.empty())
        {
            decl->attributes = attrs;
            decl->loc = startLoc; // the declaration's location is the first `@`
        }

        return decl;
    }

    // =============================================================================
    // parseFile — the entry point
    // =============================================================================

    ModuleAST *parseFile(std::string_view path,
                         std::string_view source,
                         ParserContext &ctx)
    {
        // ─── Intern the path ───────────────────────────────────────────────
        const InternedString filePath = ctx.pool.intern(path);

        // ─── Tag diagnostics with the file ─────────────────────────────────
        // The guard sets the engine's current file and restores the
        // previous value on destruction. Nested parses (an LSP analyzing
        // a background file) do not leak the tag.
        ScopedDiagnosticFile guard(ctx, filePath);

        // ─── Lex the source ────────────────────────────────────────────────
        // The lexer produces a fresh token vector ending with EOF. The
        // stream is local to this parse; the context's `stream` field is
        // a leftover from an earlier design and is not used.
        std::vector<Token> tokens =
            lexer::tokenize(source, ctx.pool, ctx.diag);
        TokenStream stream(std::move(tokens));

        // ─── The top-level loop ────────────────────────────────────────────
        auto decls = ctx.arena.makeBuilder<DeclAST *>();

        // The top-level stop set. Stops on any token that can begin a
        // top-level declaration at brace depth 0.
        const auto topLevelStop = [](TokenStream &s, int depth)
        {
            if (depth != 0)
            {
                return false;
            }
            return canStartTopDecl(s.peekType());
        };

        while (!stream.isAtEnd() && ctx.canContinue())
        {
            // ─── Try to parse a declaration ────────────────────────────────
            if (canStartTopDecl(stream.peekType()))
            {
                DeclAST *decl = parseDecl(stream, ctx);
                if (decl)
                {
                    decls.push_back(decl);
                    continue;
                }
                // parseDecl returned nullptr (it reported the error).
                // Fall through to recovery.
            }
            else
            {
                // The current token cannot begin a declaration. Report it
                // once; the token will be consumed by the recovery scan
                // below.
                ctx.diag.errorAt(DiagCode::Syntax_ExpectedDeclaration,
                                 stream.currentLoc(),
                                 "expected a declaration at the top level");
            }

            // ─── Recover ───────────────────────────────────────────────────
            const SyncResult result =
                synchronizeUntilDepth(stream, topLevelStop);

            if (result == SyncResult::ForeignCloser)
            {
                ctx.diag.errorAt(DiagCode::Syntax_UnexpectedToken,
                                 stream.currentLoc(),
                                 "unexpected '}' at the top level");
                stream.consume();
                continue;
            }

            if (result == SyncResult::ReachedEnd)
            {
                break;
            }
        }

        // ─── Build the module ──────────────────────────────────────────────
        ModuleAST *module =
            ctx.arena.make<ModuleAST>(filePath, decls.build());
        module->loc = SourceLocation{1, 1};
        module->hasErrors = ctx.diag.hasErrors();
        return module;
    }

} // namespace lucid::parser