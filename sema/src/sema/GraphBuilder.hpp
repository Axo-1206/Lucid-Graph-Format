/// @file sema/src/sema/GraphBuilder.hpp
///
/// @brief Pass 4 of Sema: build the Graph from a resolved module.
///
/// ─── What Pass 4 does ─────────────────────────────────────────────────────
/// Walks a fully-resolved, type-checked module and produces a Graph:
///
///   - One NodeInstance per node declaration.
///   - One Resource and one ResourceField list per resource declaration.
///   - One Arg per node argument.
///   - A string pool for string literals.
///   - phase_order: action nodes, sorted by (phase, source order).
///   - value_order: value nodes, topologically sorted.
///
/// ─── What Pass 4 does not do ──────────────────────────────────────────────
/// It does not load imports (Step 7.8). It does not compute the registry
/// fingerprint (Step 7.6e). It does not re-run any check; the module is
/// assumed to be error-free when this pass runs.
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

namespace lucid::sema
{

    /// @brief Build a Graph from a resolved, type-checked module.
    ///
    /// Preconditions:
    ///   - `module` is non-null and its declarations have been resolved
    ///     (Pass 2) and type-checked (Pass 3).
    ///   - `symbols` is the module's symbol table.
    ///   - `resolutions` is the module's resolution map.
    ///   - `constants` is the module's constant-value map.
    ///   - `types` is the module's type map.
    ///   - `registry` is the engine's registry.
    ///   - No prior pass reported errors. If any did, this function's
    ///     output is undefined.
    ///
    /// Postconditions:
    ///   - On success, returns a non-null Graph.
    ///   - On a cycle among value nodes, reports a Type_Cycle diagnostic
    ///     and returns a Graph that is missing the cyc nodes from
    ///     value_order.
    std::unique_ptr<Graph> buildGraph(const ModuleAST *module,
                                      const SymbolTable &symbols,
                                      const ResolutionMap &resolutions,
                                      const ConstantValueMap &constants,
                                      const TypeMap &types,
                                      const Registry &registry,
                                      StringPool &pool,
                                      lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema