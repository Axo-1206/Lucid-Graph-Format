/// @file serialization/Serializer.hpp
///
/// @brief Write a Graph to a .lucgraph byte vector.
///
/// ─── What serialize does ──────────────────────────────────────────────────
/// Walks a `sema::Graph` and produces a `std::vector<uint8_t>` in the
/// .lucgraph format. The graph is not modified. Two calls with the
/// same graph produce byte-identical vectors.
///
/// ─── What serialize does not do ───────────────────────────────────────────
/// It does not validate the graph against a registry. It does not
/// check that node type IDs exist. It writes the graph it is given.
/// Validation is the deserializer's job, at load time.
///
/// ─── The string pool ──────────────────────────────────────────────────────
/// `serialize` builds the file's string pool from two sources, in
/// order:
///
///   1. The graph's literal prefix: `graph.string_pool[0 ..
///      graph.literal_pool_size]`. These bytes are copied first, so
///      every `Literal::String`'s (offset, length) remains valid in
///      the file unchanged.
///   2. The graph's names: resource names, resource-field names, and
///      TypeId names. Each is interned once, after the prefix.
///
/// The graph itself is not modified. `Graph::literal_pool_size` is the
/// contract that makes step 1 correct; see its doc comment in
/// `sema/Graph.hpp`.

#pragma once

#include "sema/Graph.hpp"

#include <cstdint>
#include <vector>

namespace lucid::serialization
{

    /// @brief Serialize a Graph to .lucgraph bytes.
    ///
    /// @param graph  The graph to write. Not modified.
    /// @return       The file's bytes, ready to be written to disk.
    std::vector<uint8_t> serialize(const lucid::sema::Graph &graph);

} // namespace lucid::serialization