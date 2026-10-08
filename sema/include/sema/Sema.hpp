/// @file sema/Sema.hpp
///
/// @brief The public entry points of the semantic analyzer.
///
/// ─── Two entry points ─────────────────────────────────────────────────────
/// `compile` takes source text, lexes and parses it, and runs Sema. It
/// is the primary public API.
///
/// `compileModule` takes an already-parsed ModuleAST and runs Sema on
/// it. It is the same operation minus the parse. It is used by callers
/// that already have an AST — an LSP, an editor, a REPL, and Sema's own
/// tests.
///
/// Both return a CompileResult: a Graph on success, diagnostics
/// otherwise.
///
/// ─── What Sema does ───────────────────────────────────────────────────────
/// Sema takes the parser's AST and the engine's Registry and produces a
/// Graph. It performs nine steps, in order:
///
///   1. Load imports (via the CompileOptions::loadModule callback).
///   2. Collect symbols, per module.
///   3. Resolve names, per module.
///   4. Type-check every node and value.
///   5. Enforce the Event rules.
///   6. Expand composites.
///   7. Detect cycles among value nodes and among composites.
///   8. Detect dead code (unused value nodes, un-on'd action nodes).
///   9. Compute execution order and build the Graph.
///
/// If any step reports an error, subsequent steps are skipped. The
/// CompileResult has ok = false and diagnostics populated.
#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "sema/Graph.hpp"
#include "sema/Registry.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::sema
{

    // ─── Compile options ──────────────────────────────────────────────────────

    /// @brief Options for a compile.
    struct CompileOptions
    {
        /// Called by Sema when it encounters an import. Given the dotted
        /// module path (e.g., "core.keys"), returns the module's source
        /// text or std::nullopt if the module cannot be found.
        ///
        /// If not set, Sema reports an error the first time an import
        /// is encountered.
        std::function<std::optional<std::string>(std::string_view)> loadModule;
    };

    // ─── Compile result ───────────────────────────────────────────────────────

    /// @brief The result of a compile.
    struct CompileResult
    {
        /// True if Sema produced a Graph.
        bool ok = false;

        /// The graph. Valid only if ok is true.
        std::unique_ptr<Graph> graph;

        /// The diagnostics from parsing and analysis. Populated whether
        /// or not the compile succeeded; empty when there were no
        /// problems.
        std::vector<lucid::diag::Diagnostic> diagnostics;
    };

    // ─── The public entry points ──────────────────────────────────────────────

    /// @brief Compile a source file into a Graph.
    ///
    /// Lexes and parses the source, then runs Sema. Imports are resolved
    /// via options.loadModule.
    ///
    /// @param source    The source text to compile.
    /// @param filename  The file name, used in diagnostics.
    /// @param registry  The engine's registered types and node types.
    /// @param options   Optional compile options.
    CompileResult compile(std::string_view source,
                          std::string_view filename,
                          const Registry &registry,
                          CompileOptions options = {});

    /// @brief Compile an already-parsed ModuleAST into a Graph.
    ///
    /// The module must have been produced by parsing `source`. Sema
    /// uses the source for diagnostics only.
    ///
    /// @param module    The parsed module.
    /// @param source    The original source text, for diagnostics.
    /// @param filename  The file name, overrides the module's filePath
    ///                  for diagnostic purposes.
    /// @param registry  The engine's registered types and node types.
    /// @param options   Optional compile options.
    CompileResult compileModule(const ModuleAST *module,
                                std::string_view source,
                                std::string_view filename,
                                const Registry &registry,
                                CompileOptions options = {});

} // namespace lucid::sema
