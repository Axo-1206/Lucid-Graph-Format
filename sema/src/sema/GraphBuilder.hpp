/// @file sema/src/sema/GraphBuilder.hpp
///
/// @brief Pass 4 of Sema: build the Graph from resolved modules.
///
/// ─── What Pass 4 does ─────────────────────────────────────────────────────
/// Walks fully-resolved, type-checked modules and produces a single Graph:
///
///   - One NodeInstance per node declaration across all modules.
///   - One Resource and one ResourceField list per resource declaration.
///   - One Arg per node argument.
///   - A string pool for string literals.
///   - phase_order: action nodes, sorted by (phase, source order).
///   - value_order: value nodes, topologically sorted.
///
/// ─── What Pass 4 does not do ──────────────────────────────────────────────
/// It does not load imports. It does not compute the registry fingerprint.
/// It does not re-run any check; the modules are assumed to be error-free
/// when this pass runs.
///
/// ─── Error reporting ──────────────────────────────────────────────────────
/// The only error this pass reports is a cycle among value nodes. Every
/// other failure mode (unknown node type, wrong argument count, missing
/// name) is caught by an earlier pass, which stops the pipeline before
/// Pass 4 runs.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "sema/ConstantValueMap.hpp"
#include "sema/Graph.hpp"
#include "sema/Registry.hpp"
#include "sema/ResolutionMap.hpp"
#include "sema/SymbolTable.hpp"
#include "sema/TypeMap.hpp"

#include <memory>
#include <vector>

namespace lucid::sema
{

    /// @brief One module's context for graph construction.
    ///
    /// Bundles a module's AST with the per-module semantic data that the
    /// graph builder needs: the symbol table, the resolution map, the
    /// constant-value map, and the type map. One `ModuleContext` per
    /// module in the import graph.
    struct ModuleContext
    {
        ModuleAST              *module;
        const SymbolTable      *symbols;
        const ResolutionMap    *resolutions;
        const ConstantValueMap *constants;
        const TypeMap          *types;
    };

    /// @brief Build a Graph from multiple resolved, type-checked modules.
    ///
    /// Preconditions:
    ///   - Each module in `modules` is non-null and its declarations have
    ///     been resolved (Pass 2) and type-checked (Pass 3).
    ///   - All per-module pointer fields in each `ModuleContext` are non-null.
    ///   - `registry` is the engine's registry.
    ///   - No prior pass reported errors. If any did, this function's output
    ///     is undefined.
    ///
    /// Postconditions:
    ///   - On success, returns a non-null Graph containing nodes and
    ///     resources from every module, in the order the modules appear.
    ///   - On a cycle among value nodes, reports a Type_Cycle diagnostic
    ///     and returns a Graph that is missing the cyclic nodes from
    ///     value_order.
    std::unique_ptr<Graph> buildGraphFromModules(
        const std::vector<ModuleContext> &modules,
        const Registry &registry,
        StringPool &pool,
        lucid::diag::DiagnosticEngine &diag);

    /// @brief Build a Graph from a single resolved, type-checked module.
    ///
    /// Convenience wrapper around buildGraphFromModules for the single-
    /// module case and for existing tests. The semantics are identical
    /// to those of buildGraphFromModules with a one-element vector.
    std::unique_ptr<Graph> buildGraph(const ModuleAST *module,
                                      const SymbolTable &symbols,
                                      const ResolutionMap &resolutions,
                                      const ConstantValueMap &constants,
                                      const TypeMap &types,
                                      const Registry &registry,
                                      StringPool &pool,
                                      lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema