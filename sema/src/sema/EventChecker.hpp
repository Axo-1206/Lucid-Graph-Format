/// @file sema/src/sema/EventChecker.hpp
///
/// @brief Pass 4 of Sema: enforce the Event rules.
///
/// ─── What Pass 4 does ─────────────────────────────────────────────────────
/// Checks three rules at the module level:
///
///   1. An `on` clause's bare-identifier target must resolve to a
///      trigger node.
///   2. An action node must have at least one `on` clause.
///   3. No node port declares the Event type (defensive; the registry
///      should not have such a port).
///
/// ─── What Pass 4 defers ───────────────────────────────────────────────────
/// Two rules require composite expansion and are checked in Step 7.6:
///
///   - An `on` clause that targets a composite Event output
///     (`player_health.on_death`).
///   - A composite Event output's value resolving to a trigger node.
///
/// ─── Inputs ───────────────────────────────────────────────────────────────
/// The module AST, the symbol table (to find trigger declarations), the
/// resolution map (to find what `on` targets resolve to), and the
/// registry (to look up node type kinds).

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "sema/Registry.hpp"
#include "sema/ResolutionMap.hpp"
#include "sema/SymbolTable.hpp"

namespace lucid::sema
{

    /// @brief Enforce the module-level Event rules.
    ///
    /// Preconditions:
    ///   - `module` is non-null.
    ///   - `symbols` is the module's symbol table (Pass 1).
    ///   - `resolutions` is the module's resolution map (Pass 2).
    ///   - `registry` is the engine's registry.
    ///
    /// Postconditions:
    ///   - Diagnostics are reported for violations of the module-level
    ///     Event rules.
    void checkEvents(const ModuleAST *module,
                     const SymbolTable &symbols,
                     const ResolutionMap &resolutions,
                     const Registry &registry,
                     lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema