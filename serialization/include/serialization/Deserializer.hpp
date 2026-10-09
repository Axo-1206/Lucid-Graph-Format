/// @file serialization/Deserializer.hpp
///
/// @brief Read a .lucgraph byte buffer into a Graph.
///
/// ─── What deserialize does ────────────────────────────────────────────────
/// Validates the header (magic, format version, registry fingerprint),
/// walks the section table, reads each section into a fresh `Graph`,
/// and returns it. On any failure, returns a `DeserializeResult` whose
/// `ok` is false and whose `diagnostics` explains why.
///
/// ─── The fingerprint check ────────────────────────────────────────────────
/// The header carries the fingerprint of the registry the graph was
/// compiled against. `deserialize` computes the fingerprint of the
/// caller's registry and refuses on a mismatch. The check happens
/// immediately after the header, before any section is read.
///
/// ─── What deserialize does not do ─────────────────────────────────────────
/// It does not re-verify node types against the registry. The
/// fingerprint guarantees every node type ID in the file refers to the
/// same type the compiler saw. If the fingerprint matches, the IDs are
/// valid by construction.

#pragma once

#include "serialization/ByteSpan.hpp"

#include "core/diagnostics/Diagnostic.hpp"
#include "sema/Graph.hpp"
#include "sema/Registry.hpp"

#include <memory>
#include <vector>

namespace lucid::serialization
{

    /// @brief The result of a deserialize call.
    ///
    /// `ok` is true if and only if `graph` is non-null. `diagnostics`
    /// is populated on failure and may contain warnings on success (it
    /// does not today; the field exists for forward compatibility).
    struct DeserializeResult
    {
        bool ok = false;
        std::unique_ptr<lucid::sema::Graph> graph;
        std::vector<lucid::diag::Diagnostic> diagnostics;
    };

    /// @brief Read a .lucgraph byte buffer into a Graph.
    ///
    /// @param bytes     The file's bytes.
    /// @param registry  The loader's registry. Its fingerprint is
    ///                  checked against the file's.
    /// @return          A `DeserializeResult`.
    DeserializeResult deserialize(ByteSpan bytes,
                                  const lucid::sema::Registry &registry);

} // namespace lucid::serialization