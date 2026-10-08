/// @file sema/src/sema/TypeChecker.hpp
///
/// @brief Pass 3 of Sema: assign types and check consistency.
///
/// ─── What Pass 3 does ─────────────────────────────────────────────────────
/// Walks the resolved module and, for every expression:
///   - Assigns a TypeId.
///   - Checks that uses are consistent: argument types match port
///     types, resource defaults match field types, node expressions
///     name existing node types, etc.
///
/// Produces a TypeMap.
///
/// ─── What Pass 3 does not do ──────────────────────────────────────────────
/// It does not enforce the Event rules (that is Pass 4), does not
/// expand composites (Pass 6), does not detect cycles (Pass 7), and
/// does not build the graph (Pass 9).

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "sema/Registry.hpp"
#include "sema/ResolutionMap.hpp"
#include "sema/SymbolTable.hpp"
#include "sema/TypeMap.hpp"

namespace lucid::sema
{

    /// @brief Assign types and check every use in a module.
    ///
    /// Preconditions:
    ///   - `module` is non-null.
    ///   - `symbols` is the module's symbol table (Pass 1).
    ///   - `resolutions` is the module's resolution map (Pass 2).
    ///   - `types` is empty or its prior contents are to be overwritten.
    ///
    /// Postconditions:
    ///   - Every expression in the module is recorded in `types`.
    ///   - Diagnostics are reported for type errors.
    void checkTypes(const ModuleAST* module,
                    const SymbolTable& symbols,
                    const ResolutionMap& resolutions,
                    const Registry& registry,
                    TypeMap& types,
                    lucid::diag::DiagnosticEngine& diag);

} // namespace lucid::sema
