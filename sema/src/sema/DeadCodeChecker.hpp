/// @file sema/src/sema/DeadCodeChecker.hpp
///
/// @brief Pass 5 of Sema: report unused value nodes.
///
/// ─── What Pass 5 does ─────────────────────────────────────────────────────
/// Walks the module's node declarations and reports a Warn_DeadNode
/// warning for every Value node that no other node references.
///
/// ─── What "used" means ────────────────────────────────────────────────────
/// A Value node is used if any other node's argument is an identifier
/// that resolves to it. The check does not attempt transitive
/// reachability from action nodes; a Value node used only by another
/// unused Value node is not reported.
///
/// ─── Why warnings, not errors ─────────────────────────────────────────────
/// A user might declare a value node in advance and wire it up in a
/// later edit. Reporting an error would block the compile. A warning
/// informs the user without stopping them.
///
/// ─── Why this pass walks the AST, not the graph ───────────────────────────
/// The check reports at a node declaration's source location. The AST
/// has the location; the graph has the index. Walking the AST lets the
/// diagnostic point at the declaration.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "sema/Registry.hpp"
#include "sema/ResolutionMap.hpp"
#include "sema/SymbolTable.hpp"

namespace lucid::sema
{

    /// @brief Report unused Value nodes.
    ///
    /// Preconditions:
    ///   - `module` is non-null and has been fully resolved and checked.
    ///   - `symbols`, `resolutions`, and `registry` are the same ones
    ///     used by the earlier passes.
    ///   - The module has no errors (a compile with errors does not
    ///     reach this pass).
    ///
    /// Postconditions:
    ///   - Diagnostics are reported for unused Value nodes. Warnings
    ///     only; the module is not modified.
    void checkDeadCode(const ModuleAST *module,
                       const ResolutionMap &resolutions,
                       const Registry &registry,
                       lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema