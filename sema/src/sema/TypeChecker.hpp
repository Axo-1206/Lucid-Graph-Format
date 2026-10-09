/// @file sema/src/sema/TypeChecker.hpp
///
/// @brief Pass 3 of Sema: assign types, check consistency, and enforce
///        the trigger rules.
///
/// ─── What Pass 3 does ─────────────────────────────────────────────────────
/// Walks the resolved module and:
///
///   - Assigns a TypeId to every expression.
///   - Checks that uses are consistent: argument types match the
///     declared argument types, resource defaults match field types,
///     node expressions name existing node types.
///   - Enforces the two trigger rules:
///       1. An `on` clause's target must resolve to a trigger node.
///       2. An action node must have at least one `on` clause.
///
/// Produces a TypeMap.
///
/// ─── Why the trigger rules live here ──────────────────────────────────────
/// The trigger rules need the same inputs the type checker already
/// has: the module, the symbol table, the registry, and the
/// diagnostic engine. They produce no output; they only report. The
/// type checker already walks every node declaration, so the rules
/// run during that walk. A separate pass would duplicate the walk and
/// add a file for two rules.
///
/// ─── Why this is not called "EventChecker" ────────────────────────────────
/// The format has no Event type and no composite nodes. A trigger
/// node is the only thing an `on` clause can target. The rules are
/// about trigger nodes, not about an Event type.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "sema/Registry.hpp"
#include "sema/ResolutionMap.hpp"
#include "sema/SymbolTable.hpp"
#include "sema/TypeMap.hpp"

namespace lucid::sema
{

    /// @brief Assign types, check uses, and enforce the trigger rules.
    ///
    /// Preconditions:
    ///   - `module` is non-null.
    ///   - `symbols` is the module's symbol table (Pass 1).
    ///   - `resolutions` is the module's resolution map (Pass 2).
    ///   - `types` is empty or its prior contents are to be overwritten.
    ///
    /// Postconditions:
    ///   - Every expression in the module is recorded in `types`.
    ///   - Diagnostics are reported for type errors and for violations
    ///     of the trigger rules.
    void checkTypes(const ModuleAST *module,
                    const SymbolTable &symbols,
                    const ResolutionMap &resolutions,
                    const Registry &registry,
                    TypeMap &types,
                    lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema