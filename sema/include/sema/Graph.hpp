/// @file sema/Graph.hpp
///
/// @brief The output of Sema: a flat, resolved graph.
///
/// ─── What a Graph is ──────────────────────────────────────────────────────
/// The engine's runtime input. A graph has nodes, arguments, resources,
/// subscriber lists, two execution orders, a string pool, and the
/// registry fingerprint it was compiled against.
///
/// ─── Flat-vector layout ───────────────────────────────────────────────────
/// All arrays are single flat vectors. A NodeInstance slices the shared
/// args and subscribers vectors with (offset, count) pairs. A Resource
/// slices the shared resource_fields vector the same way. This is one
/// allocation per vector instead of one per node, and it is
/// cache-friendly.
///
/// ─── Ownership ────────────────────────────────────────────────────────────
/// The Graph owns all of its vectors. `compile` returns it in a
/// `std::unique_ptr<Graph>`. When the pointer is destroyed, the Graph
/// and all of its memory are freed.

#pragma once

#include "core/memory/ArenaSpan.hpp"
#include "sema/Literal.hpp"
#include "sema/TypeId.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace lucid::sema
{

    using NodeId = uint32_t;
    using NodeIndex = uint32_t;

    // ─── Argument ─────────────────────────────────────────────────────────────

    /// @brief One argument of a node, in the compiled graph.
    ///
    /// An argument is one of three things:
    ///
    ///   Literal      — a compile-time constant. An enum member such as
    ///                  `Key.A` is resolved by Sema to an integer, so it
    ///                  appears here as a Literal with an integer kind.
    ///                  The graph stores the integer, not the member name.
    ///   NodeRef      — a reference to another node's result, by index
    ///                  into Graph::nodes.
    ///   ResourceRef  — a reference to a resource field, by the pair
    ///                  (resource index, field index).
    ///
    /// There is no separate "enum member" kind. An enum member is a
    /// literal after Sema resolves it.
    struct Arg
    {
        enum class Kind : uint8_t
        {
            Literal,
            NodeRef,
            ResourceRef,
        };

        Kind kind = Kind::Literal;

        union
        {
            Literal literal;
            NodeIndex node_ref;
            struct
            {
                uint32_t resource_index;
                uint32_t field_index;
            } resource_ref;
        };

        Arg() : kind(Kind::Literal), literal() {}

        static Arg makeLiteral(Literal lit)
        {
            Arg a;
            a.kind = Kind::Literal;
            a.literal = lit;
            return a;
        }

        static Arg makeNodeRef(NodeIndex idx)
        {
            Arg a;
            a.kind = Kind::NodeRef;
            a.node_ref = idx;
            return a;
        }

        static Arg makeResourceRef(uint32_t resourceIndex, uint32_t fieldIndex)
        {
            Arg a;
            a.kind = Kind::ResourceRef;
            a.resource_ref.resource_index = resourceIndex;
            a.resource_ref.field_index = fieldIndex;
            return a;
        }
    };

    // ─── Node instance ────────────────────────────────────────────────────────

    struct NodeInstance
    {
        NodeId type_id = 0;
        uint32_t phase = 0;

        uint32_t args_offset = 0;
        uint32_t args_count = 0;

        uint32_t subscribers_offset = 0;
        uint32_t subscribers_count = 0;
    };

    // ─── Resource ─────────────────────────────────────────────────────────────

    struct Resource
    {
        std::string_view name;

        uint32_t fields_offset = 0;
        uint32_t fields_count = 0;
    };

    /// @brief One field of a resource.
    ///
    /// `defaultValue` is a Literal, not a general Arg. A field's default
    /// must be a compile-time constant: a literal, an enum member
    /// (resolved to an integer), or a node expression that Sema
    /// constant-folds to a literal. A default that cannot be reduced to
    /// a literal is a Sema error (`Type_InvalidDefault`).
    ///
    /// `hasDefault` distinguishes "no default written" from "a default
    /// written as the field type's zero value." A field with no default
    /// has `hasDefault == false` and is zero-initialized at load time.
    /// A field with an explicit zero (`hp: int = 0`) has
    /// `hasDefault == true` and `defaultValue` holding the zero literal.
    struct ResourceField
    {
        std::string_view name;
        TypeId type;
        Literal defaultValue;
        bool hasDefault = false;
    };

    // ─── Graph ────────────────────────────────────────────────────────────────

    struct Graph
    {
        std::vector<NodeInstance> nodes;
        std::vector<Arg> args;
        std::vector<Resource> resources;
        std::vector<ResourceField> resource_fields;
        std::vector<NodeIndex> subscribers;
        std::vector<NodeIndex> phase_order;
        std::vector<NodeIndex> value_order;
        std::vector<char> string_pool;

        uint64_t registry_fingerprint = 0;

        // ─── Convenience accessors ────────────────────────────────────────

        ArenaSpan<Arg> argsOf(const NodeInstance &node) const noexcept
        {
            return ArenaSpan<Arg>(
                args.data() + node.args_offset, node.args_count);
        }

        ArenaSpan<ResourceField> fieldsOf(const Resource &res) const noexcept
        {
            return ArenaSpan<ResourceField>(
                resource_fields.data() + res.fields_offset, res.fields_count);
        }

        ArenaSpan<NodeIndex> subscribersOf(const NodeInstance &node) const noexcept
        {
            return ArenaSpan<NodeIndex>(
                subscribers.data() + node.subscribers_offset,
                node.subscribers_count);
        }
    };

} // namespace lucid::sema