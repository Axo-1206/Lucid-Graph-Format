/// @file sema/src/sema/ImportResolver.hpp
///
/// @brief Loads a module's transitively-imported modules.
///
/// ─── What this does ───────────────────────────────────────────────────────
/// Given a root module (already parsed), walks its import declarations,
/// loads each imported module via the CompileOptions::loadModule
/// callback, parses it, and recurses into that module's imports. The
/// result is a ModuleSet in dependency order (a module's dependencies
/// precede it).
///
/// ─── Cycle detection ──────────────────────────────────────────────────────
/// The import graph must be acyclic. A cycle is reported as
/// Import_Circular.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "sema/Sema.hpp"

#include <string_view>
#include <vector>

namespace lucid::sema
{

    /// @brief The set of modules loaded from a root module's imports.
    struct ModuleSet
    {
        /// Modules in dependency order: a module appears after every
        /// module it imports. The root module is the last element.
        std::vector<ModuleAST *> modules;
    };

    /// @brief Load a module and its transitive imports.
    ///
    /// Preconditions:
    ///   - `rootModule` is non-null and has been parsed.
    ///   - `rootPath` is the root module's path.
    ///   - `options.loadModule` may be null (a module with no imports
    ///     works without a loader).
    ///   - `pool` and `arena` are the session's.
    ///
    /// Postconditions:
    ///   - On success, returns a ModuleSet with every loaded module in
    ///     dependency order.
    ///   - On a cycle among modules, reports Import_Circular.
    ///   - On a module that cannot be loaded, reports
    ///     Import_ModuleNotFound.
    ModuleSet resolveImports(ModuleAST *rootModule,
                             std::string_view rootPath,
                             const CompileOptions &options,
                             StringPool &pool,
                             ASTArena &arena,
                             lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema