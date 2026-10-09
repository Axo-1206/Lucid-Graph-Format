/// @file sema/src/sema/AttributeChecker.hpp
///
/// @brief Sema's attribute validation pass.
///
/// ─── What this does ───────────────────────────────────────────────────────
/// Walks every top-level declaration's attribute list and reports:
///
///   - Attr_Unknown: an attribute name that is not recognized.
///   - Attr_ExportOnImport: @export on an import declaration.
///   - Attr_Duplicate: the same attribute twice on one declaration.
///
/// @export is legal on enum, resource, and node. It is not legal on
/// import.
///
/// ─── Why a separate pass ──────────────────────────────────────────────────
/// Attribute checking needs only the AST and the diagnostic engine. It
/// does not need the symbol table, the registry, or any resolution. It
/// runs once per module, before Pass 1, so that a bad attribute is
/// reported before any pass that might produce cascading errors.
///
/// ─── What this does not do ────────────────────────────────────────────────
/// It does not check attribute arguments (there are none today) and does
/// not interpret @export's meaning. Interpretation is the export-merge
/// step's concern.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/StringPool.hpp"

namespace lucid::sema
{

    /// @brief Validate a module's attribute lists.
    ///
    /// Preconditions:
    ///   - `module` is non-null.
    ///   - `pool` is the session's StringPool.
    ///
    /// Postconditions:
    ///   - Diagnostics are reported for unknown, misplaced, or duplicate
    ///     attributes.
    void checkAttributes(const ModuleAST *module,
                         StringPool &pool,
                         lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema