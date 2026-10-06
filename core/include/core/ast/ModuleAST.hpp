/// @file core/ast/ModuleAST.hpp
///
/// @brief The root of a parsed file.
///
/// ─── What a module is ─────────────────────────────────────────────────────
/// A file is a module. The grammar's §2.1 writes:
///
///     program ::= { top_decl }
///
/// The top level contains only declarations. There are no statements, no
/// executable code, no initialization. The `decls` span holds every
/// top-level declaration in source order, including imports.
///
/// ─── Why one span, not one list per kind ──────────────────────────────────
/// The grammar allows any declaration to appear in any order. Keeping the
/// declarations in a single source-ordered span lets the formatter emit
/// them in the order the source wrote them, and lets the parser append as
/// it reads. A consumer that wants just the imports, just the enums, or
/// just the nodes filters the span with isa<T>().
///
/// ─── What the parser puts here ────────────────────────────────────────────
///   - filePath:   set by the caller (the parser receives it as an argument).
///   - decls:      one entry per top-level declaration, in source order.
///   - hasErrors:  true if the parser reported any syntax errors.
///
/// ─── What the parser does not put here ────────────────────────────────────
/// Diagnostics go to the DiagnosticEngine; the ModuleAST holds only the
/// summary flag. Resolved imports go to a resolver's own map; the AST
/// holds only the ImportDeclASTs the source wrote. Later passes do not
/// write their results back onto the AST.

#pragma once

#include "core/ast/BaseAST.hpp"
#include "core/ast/DeclAST.hpp"
#include "core/memory/ArenaSpan.hpp"
#include "core/memory/InternedString.hpp"

/// @brief The root of a parsed file.
///
/// The parser produces one of these per source file. The formatter reads
/// one and emits text; a future compiler's Sema reads one and produces a
/// graph.
struct ModuleAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::Module;

    /// The file's path, interned. Set by the caller. Used by diagnostics
    /// that want to name the file.
    InternedString filePath;

    /// The top-level declarations, in source order. Imports are included
    /// in this span, at their source position.
    ArenaSpan<DeclAST *> decls;

    /// True if the parser reported any syntax errors while parsing this
    /// module. The diagnostics themselves are in the DiagnosticEngine;
    /// this flag is a summary.
    bool hasErrors = false;

    ModuleAST() : BaseAST(ASTKind::Module) {}

    ModuleAST(InternedString path, ArenaSpan<DeclAST *> d)
        : BaseAST(ASTKind::Module), filePath(path), decls(d) {}

    // ─── Convenience queries ────────────────────────────────────────────

    /// True if the module has no top-level declarations.
    bool isEmpty() const { return decls.empty(); }

    /// The number of top-level declarations.
    size_t declCount() const { return decls.size(); }
};