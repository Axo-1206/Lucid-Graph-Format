/// @file sema/NodeKind.hpp
///
/// @brief The three kinds of node.
///
/// ─── What a node kind is ──────────────────────────────────────────────────
/// Every node type registered by the engine declares one of three kinds.
/// The kind determines what the node can do and what Sema checks about it.
///
///   Value   — produces a value; no side effects.
///   Action  — performs a side effect; must have at least one `on` clause.
///   Trigger — emits events; can be the target of an `on` clause.
///
/// ─── What the kind is not ─────────────────────────────────────────────────
/// It is not the same as the node's *category*. The category is a
/// free-form string ("Physics", "Flow", "Math") that the engine chooses
/// for organizational purposes. Sema never reads the category. See
/// Registry.hpp's NodeTypeInfo for the distinction.
///
/// ─── Who defines the kind ─────────────────────────────────────────────────
/// The format library defines this enum and the rules that use it.
/// The engine assigns each node type its kind when registering the node.
/// Sema reads the kind during analysis. See the consolidated design for
/// the full split.
#pragma once

#include <cstdint>

namespace lucid::sema
{

    /// @brief The kind of a node type.
    enum class NodeKind : uint8_t
    {
        /// Produces a value. Has one or more output ports. Is evaluated
        /// on demand. Must be used somewhere in the graph, or Sema
        /// reports it as dead code.
        Value,

        /// Performs a side effect. Has no outputs. Must have at least
        /// one `on` clause. Is executed in phase order.
        Action,

        /// Emits events. Has no outputs but may have payload fields.
        /// Can be the target of an `on` clause. Cannot be `on`-targeted
        /// by itself.
        Trigger,
    };

    /// @brief The name of a node kind, for diagnostics and JSON.
    inline const char* nodeKindName(NodeKind k) noexcept
    {
        switch (k)
        {
        case NodeKind::Value:   return "Value";
        case NodeKind::Action:  return "Action";
        case NodeKind::Trigger: return "Trigger";
        }
        return "Unknown";
    }

} // namespace lucid::sema
