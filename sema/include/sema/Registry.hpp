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

    // ─── Node arguments ───────────────────────────────────────────────────────

    /// @brief One declared argument of a node type.
    ///
    /// The grammar's `node_expr` supplies arguments positionally:
    /// `NodeType(arg0, arg1, ...)`. The name is used for diagnostics only;
    /// a call site does not name its arguments.
    struct NodeArgInfo
    {
        std::string_view name;
        TypeId type;
    };

    // ─── Node types ───────────────────────────────────────────────────────────

    /// @brief One node type declared by the engine.
    ///
    /// A node type has a name, a kind, a category, a phase, and a list of
    /// positional arguments. A Value node also has a result type: the type
    /// of the value it produces. Action and Trigger nodes have no result;
    /// their `resultType` is left invalid.
    ///
    /// There is no separate "input" and "output" port model. A node's
    /// arguments are what the grammar's call site supplies. The result is
    /// what a Value node produces.
    struct NodeTypeInfo
    {
        std::string_view name;
        NodeKind kind;
        std::string_view category;
        uint32_t phase = 0;

        /// The declared arguments, in call-site order. The grammar supplies
        /// arguments positionally; Sema checks count and types against this
        /// span.
        ArenaSpan<NodeArgInfo> args;

        /// The result type of a Value node. Invalid for Action and Trigger
        /// nodes. A Value node with an invalid `resultType` is a registry
        /// error; Sema reports it when the type checker reads the result.
        TypeId resultType;
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