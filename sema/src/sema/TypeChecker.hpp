/// @file sema/src/sema/TypeChecker.hpp
///
/// @brief Pass 3 of Sema: assign types, check consistency, enforce
///        the trigger rules, and fold constant values.
///
/// ─── What Pass 3 does ─────────────────────────────────────────────────────
/// Walks the resolved module and:
///
///   - Assigns a TypeId to every expression.
///   - Records the constant value of every literal and enum member.
///   - Checks that uses are consistent: argument types match the
///     declared argument types, resource defaults match field types,
///     node expressions name existing node types.
///   - Enforces the two trigger rules:
///       1. An `on` clause's target must resolve to a trigger node.
///       2. An action node must have at least one `on` clause.
///
/// Produces a TypeMap and a ConstantValueMap.
///
/// ─── Why the trigger rules live here ──────────────────────────────────────
/// The trigger rules need the same inputs the type checker already
/// has: the module, the symbol table, the registry, and the
/// diagnostic engine. They produce no output; they only report. The
/// type checker already walks every node declaration, so the rules
/// run during that walk.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "sema/ConstantValueMap.hpp"
#include "sema/Registry.hpp"
#include "sema/ResolutionMap.hpp"
#include "sema/SymbolTable.hpp"
#include "sema/TypeMap.hpp"

namespace lucid::sema
{

    /// @brief Assign types, check uses, fold constants, and enforce the
    ///        trigger rules.
    ///
    /// Preconditions:
    ///   - `module` is non-null.
    ///   - `symbols` is the module's symbol table (Pass 1).
    ///   - `resolutions` is the module's resolution map (Pass 2).
    ///   - `types` and `constants` are empty or their prior contents are
    ///     to be overwritten.
    ///
    /// Postconditions:
    ///   - Every expression in the module is recorded in `types`.
    ///   - Every literal and enum member is recorded in `constants`.
    ///   - Diagnostics are reported for type errors and trigger-rule
    ///     violations.
    void checkTypes(const ModuleAST *module,
                    const SymbolTable &symbols,
                    const ResolutionMap &resolutions,
                    const Registry &registry,
                    TypeMap &types,
                    ConstantValueMap &constants,
                    lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema