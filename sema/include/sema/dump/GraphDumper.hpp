/// @file sema/dump/GraphDumper.hpp
///
/// @brief Serialize a Graph to JSON.
///
/// ─── What this does ───────────────────────────────────────────────────────
/// Walks a Graph and produces a JSON document. The JSON is used by
/// fixture tests and by any tool that wants to inspect a compiled
/// graph without linking the engine.
///
/// ─── What this does not do ────────────────────────────────────────────────
/// It does not resolve node type IDs to names, does not read the
/// registry, and does not emit the AST. It is a mechanical
/// serialization of the Graph structure.
///
/// ─── Determinism ──────────────────────────────────────────────────────────
/// The output is deterministic: the same Graph always produces the
/// same bytes. Fixture tests compare the dumper's output against a
/// stored file, so determinism is required.

#pragma once

#include "sema/Graph.hpp"

#include <string>

namespace lucid::sema::dump
{

    /// @brief Serialize a Graph to JSON.
    ///
    /// @param graph  The graph to serialize.
    /// @return The JSON text (a single line, no pretty-printing).
    std::string dumpGraph(const Graph &graph);

} // namespace lucid::sema::dump