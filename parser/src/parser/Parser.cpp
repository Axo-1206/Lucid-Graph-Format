/// @file parser/src/parser/Parser.cpp
///
/// @brief Implementation of the parser's entry point and dispatcher.
///
/// ─── Current state: STUB ──────────────────────────────────────────────────
/// Both functions are stubs. They report Internal_NotImplemented and return
/// the minimum valid value for their signature. They do not consume tokens,
/// they do not lex, and they do not call the rules/ parsers.
///
/// The real implementations land in Phase 3, in this order:
///
///   - `parseDecl`: read the attribute list, dispatch on the current
///     token, attach the attributes to the returned declaration.
///   - `parseFile`: intern the path, tag diagnostics with the file, lex
///     the source, parse top-level declarations in a loop, recover on
///     failure, and build the ModuleAST.
///
/// The stub form exists so that the whole parser links and so that the
/// skeleton test (`tests/parser/test_parser_skeleton.cpp`) can assert that
/// `parseFile` returns a non-null ModuleAST* with `hasErrors == true`.

#include "parser/Parser.hpp"

#include "core/diagnostics/DiagCode.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    // =============================================================================
    // parseDecl — dispatcher (STUB)
    // =============================================================================

    /// @brief Parse one top-level declaration. STUB.
    ///
    /// The real implementation will:
    ///   1. Read the attribute list with `parseAttributeList`.
    ///   2. Look at the current token.
    ///   3. Dispatch to the matching declaration parser.
    ///   4. Attach the attribute list to the returned DeclAST.
    ///   5. Return the declaration, or nullptr if the current token cannot
    ///      begin a declaration.
    ///
    /// The stub reports NotImplemented and returns nullptr. It does not
    /// consume tokens, so the caller's recovery scan takes over immediately
    /// if `parseFile` is ever called against the stub form of `parseDecl`.
    DeclAST *parseDecl(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseDecl: not yet implemented");
        return nullptr;
    }

    // =============================================================================
    // parseFile — entry point (STUB)
    // =============================================================================

    /// @brief Parse one source file into a ModuleAST. STUB.
    ///
    /// The real implementation will:
    ///   1. Intern the path.
    ///   2. Construct a ScopedDiagnosticFile guard.
    ///   3. Lex the source into a fresh TokenStream.
    ///   4. Parse top-level declarations in a loop until EOF or an error cap.
    ///   5. Recover from a failed declaration with a stop set built from
    ///      `GrammarPositions.hpp`.
    ///   6. Build and return the ModuleAST.
    ///
    /// The stub does none of that. It interns the path, tags the diagnostic
    /// with the file, reports NotImplemented, and returns an empty module
    /// with `hasErrors == true`. It does not lex, does not loop, and does
    /// not call `parseDecl`.
    ///
    /// The stub never returns null: a caller can rely on the postcondition
    /// documented in Parser.hpp.
    ModuleAST *parseFile(std::string_view path,
                         std::string_view source,
                         ParserContext &ctx)
    {
        // Intern the path so the diagnostic and the module both carry a
        // valid file identity. This is the only real work the stub does.
        InternedString filePath = ctx.pool.intern(path);

        // Tag every diagnostic raised in this scope with the file identity.
        // The guard's destructor restores the previous file, so nested
        // parses (an LSP analyzing a background file) do not leak the tag.
        ScopedDiagnosticFile guard(ctx, filePath);

        // `source` is unused in the stub. Mark it to avoid a warning under
        // /W4 without changing the signature.
        (void)source;

        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         SourceLocation{1, 1},
                         "parseFile: not yet implemented");

        ModuleAST *module =
            ctx.arena.make<ModuleAST>(filePath, ArenaSpan<DeclAST *>{});
        module->hasErrors = true;
        return module;
    }

} // namespace lucid::parser