/// @file sema/src/sema/SymbolCollector.hpp
///
/// @brief Pass 1 of Sema: build the per-module symbol table.
///
/// ─── What Pass 1 does ─────────────────────────────────────────────────────
/// Walks a ModuleAST's declarations and adds a Symbol for each one to
/// the given SymbolTable. Reports Name_Redeclaration on duplicate names.
///
/// ─── What Pass 1 does not do ──────────────────────────────────────────────
/// It does not resolve any reference, does not look at imports, and
/// does not type-check anything. It is a single walk over the top-level
/// declarations.
///
/// ─── Why this header is internal ──────────────────────────────────────────
/// The SymbolCollector is an implementation detail of Pass 1. Only
/// Sema.cpp calls it. Later passes and tests use the SymbolTable
/// directly, not the collector.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "sema/SymbolTable.hpp"

namespace lucid::sema
{

    /// @brief Build a module's symbol table.
    ///
    /// Walks `module->decls` and adds each named declaration to `table`.
    /// Duplicate names are reported through `diag`.
    ///
    /// Preconditions:
    ///   - `module` is non-null and was produced by the parser.
    ///   - `table` is empty or contains only entries this function adds.
    ///     The function does not clear the table.
    void collectSymbols(const ModuleAST *module,
                        SymbolTable &table,
                        lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema
