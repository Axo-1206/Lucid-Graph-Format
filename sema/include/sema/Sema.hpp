/// @file sema/Sema.hpp
///
/// @brief The public entry points of the semantic analyzer.
///
/// ─── Two entry points ─────────────────────────────────────────────────────
/// `compile` takes source text, lexes and parses it, and runs Sema.
///
/// `compileModule` takes an already-parsed ModuleAST and runs Sema on
/// it. It is the same operation minus the parse. The caller provides
/// the session's StringPool; Sema interns names into it as needed.
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
///   6. Detect dead code (unused value nodes, un-on'd action nodes).
///   7. Compute execution order and build the Graph.
///
/// If any step reports an error, subsequent steps are skipped.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/StringPool.hpp"
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

    struct CompileOptions
    {
        std::function<std::optional<std::string>(std::string_view)> loadModule;
    };

    // ─── Compile result ───────────────────────────────────────────────────────

    struct CompileResult
    {
        bool ok = false;
        std::unique_ptr<Graph> graph;
        std::vector<lucid::diag::Diagnostic> diagnostics;
    };

    // ─── The public entry points ──────────────────────────────────────────────

    CompileResult compile(std::string_view source,
                          std::string_view filename,
                          const Registry &registry,
                          CompileOptions options = {});

    CompileResult compileModule(const ModuleAST *module,
                                std::string_view source,
                                std::string_view filename,
                                StringPool &pool,
                                const Registry &registry,
                                CompileOptions options = {});

} // namespace lucid::sema
