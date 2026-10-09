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
/// `serialize` augments the graph's `string_pool` on its own internal
/// copy of the offset table. The graph itself is not modified; the
/// bytes written to the file include every string the file needs.

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