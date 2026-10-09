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
/// Graph. It performs these steps, in order:
///
///   1. Load imports (via the CompileOptions::loadModule callback).
///   2. Collect symbols, per module.
///   3. Resolve names, per module.
///   4. Type-check every node and value, and enforce the trigger rules:
///      every `on` target must be a trigger node, and every action node
///      must have at least one `on` clause.
///   5. Detect dead code (unused value nodes).
///   6. Compute execution order and build the Graph.
///
/// If any step reports an error, subsequent steps are skipped.
///
/// ─── Why the trigger rules are in the type checker ────────────────────────
/// The two trigger rules need the same inputs the type checker already
/// has (the module, the symbol table, the registry, the diagnostic
/// engine), produce no output, and run during the same walk over node
/// declarations. A separate pass would duplicate the walk for two rules.
/// See TypeChecker.hpp for the full reasoning.

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
