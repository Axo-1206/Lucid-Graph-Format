/// @file sema/Registry.hpp
///
/// @brief The engine's declarations of node types, types, and phases.
///
/// ─── What the Registry is ─────────────────────────────────────────────────
/// The interface between the engine and the format library. The engine
/// fills it in at startup; Sema reads it during compilation. It is the
/// entire vocabulary the engine provides to the format library.
///
/// ─── The shape / content split ────────────────────────────────────────────
/// The format library defines the *shape* of the Registry — the structs
/// below, the NodeKind enum, the TypeId struct. The engine provides the
/// *content* — the actual node types, enum members, handle names, and
/// phases.
///
/// ─── Names are string_views ───────────────────────────────────────────────
/// Registry names are std::string_view, not InternedString. The engine
/// writes plain strings. Sema interns them into the session's pool at
/// compile time.
///
/// ─── Spans are ArenaSpan ──────────────────────────────────────────────────
/// The Registry's lists are ArenaSpan<T>: non-owning, read-only views
/// over the engine's arrays. The engine must keep those arrays alive
/// for the duration of any compile that uses the Registry.

#pragma once

#include "core/memory/ArenaSpan.hpp"
#include "sema/Literal.hpp"
#include "sema/NodeKind.hpp"
#include "sema/TypeId.hpp"

#include <cstdint>
#include <string_view>

namespace lucid::sema
{

    // ─── Phase ────────────────────────────────────────────────────────────────

    struct PhaseInfo
    {
        std::string_view name;
    };

    // ─── Enum ─────────────────────────────────────────────────────────────────

    struct EnumMemberInfo
    {
        std::string_view name;
        int64_t value;
    };

    struct EnumTypeInfo
    {
        std::string_view name;
        ArenaSpan<EnumMemberInfo> members;
    };

    // ─── Handle ───────────────────────────────────────────────────────────────

    struct HandleTypeInfo
    {
        std::string_view name;
    };

    // ─── Node ports ───────────────────────────────────────────────────────────

    struct NodePortInfo
    {
        std::string_view name;
        TypeId type;
    };

    // ─── Node types ───────────────────────────────────────────────────────────

    struct NodeTypeInfo
    {
        std::string_view name;
        NodeKind kind;
        std::string_view category;
        uint32_t phase = 0;

        ArenaSpan<NodePortInfo> inputs;
        ArenaSpan<NodePortInfo> outputs;
        ArenaSpan<NodePortInfo> payload;
    };

    // ─── The Registry ─────────────────────────────────────────────────────────

    struct Registry
    {
        ArenaSpan<PhaseInfo> phases;
        ArenaSpan<EnumTypeInfo> enums;
        ArenaSpan<HandleTypeInfo> handles;
        ArenaSpan<NodeTypeInfo> nodeTypes;
    };

    /// @brief Compute a stable fingerprint of the registry's contents.
    ///
    /// Two registries with the same declarations produce the same
    /// fingerprint; two registries with different declarations produce
    /// different fingerprints. The algorithm is defined by the
    /// implementation. Called by `compile` and by the .lucgraph
    /// deserializer.
    uint64_t computeRegistryFingerprint(const Registry &registry);

} // namespace lucid::sema