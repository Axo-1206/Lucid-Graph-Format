/// @file sema/src/sema/Resolver.hpp
///
/// @brief Pass 2 of Sema: resolve names against the symbol table.
///
/// ─── What Pass 2 does ─────────────────────────────────────────────────────
/// Walks every declaration's body and resolves every name reference
/// against the module's symbol table. Records each resolution in a
/// ResolutionMap. Reports Name_UndefinedX diagnostics for unresolved
/// names.
///
/// ─── What Pass 2 does not do ──────────────────────────────────────────────
/// It does not type-check, does not detect cycles,
/// and does not load imports. Those are later steps.
///
/// ─── The resolver's scope ─────────────────────────────────────────────────
/// The resolver runs on one module at a time. It uses the module's
/// symbol table (from Pass 1). References to import aliases are
/// recorded as deferred (null target) for Step 7.8 to fill in.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/StringPool.hpp"
#include "sema/ResolutionMap.hpp"
#include "sema/SymbolTable.hpp"
#include "sema/ModuleTable.hpp"

namespace lucid::sema
{

    /// @brief Resolve every reference in a module.
    ///
    /// Preconditions:
    ///   - `module` is non-null and was produced by the parser.
    ///   - `symbols` is the module's symbol table, populated by Pass 1.
    ///   - `resolutions` is empty or its prior contents are to be
    ///     overwritten. The function does not clear it.
    ///
    /// Postconditions:
    ///   - Every reference in the module is recorded in `resolutions`.
    ///     Non-null targets for resolved references; null targets for
    ///     deferred references (typically, references through an import
    ///     alias).
    ///   - Diagnostics are reported for references that could not be
    ///     resolved to any local symbol.
    void resolveNames(const ModuleAST *module,
                      const SymbolTable &symbols,
                      const ModuleTable &moduleTable,
                      ResolutionMap &resolutions,
                      lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema
