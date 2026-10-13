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
#include <string>
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

    /// @brief One resource in the compiled graph.
    ///
    /// `name` is owned. The Graph is a self-contained value: it can
    /// outlive the Sema session that produced it, and it can be
    /// serialized without reaching back into a StringPool.
    struct Resource
    {
        std::string name;

        uint32_t fields_offset = 0;
        uint32_t fields_count = 0;
    };

    /// @brief One field of a resource.
    ///
    /// `value` is the field's value. Resources are compile-time
    /// constants: a resource is declared in `.lucid` source, its fields
    /// are resolved at compile time, and nothing mutates a resource at
    /// runtime.
    ///
    /// Every field has a value. A field written with `= value` gets the
    /// written value; a field written without a default gets the type's
    /// zero, resolved by `zeroLiteralOf`. The two cases are
    /// indistinguishable after compilation, which is why there is no
    /// `hasDefault` flag: the source syntax has a default, but the
    /// compiled graph has only values.
    ///
    /// `value` is a `Literal`, not a general `Arg`. A field's value must
    /// be a compile-time constant: a literal, an enum member (resolved
    /// to an integer), or a node expression that Sema constant-folds to
    /// a literal. A value that cannot be reduced to a literal is a Sema
    /// error (`Type_InvalidDefault`).
    struct ResourceField
    {
        std::string name;
        TypeId type;
        Literal value;
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

        /// String literals. A `Literal` of kind `String` holds an
        /// `(offset, length)` into this pool.
        std::vector<char> string_pool;

        /// Struct constant values. A `Literal` of kind `Struct` holds an
        /// `(offset, length)` into this pool.
        ///
        /// The pool holds only *compile-time constant* struct values —
        /// the structs Sema folded from source. Struct values produced
        /// by value nodes at runtime live in the runtime's execution
        /// context, not in the graph.
        ///
        /// A struct value's bytes are its fields laid out in the
        /// struct type's declared field order. The layout of each
        /// field is the field type's own wire form (a `float32` is
        /// 4 bytes, a `Vector2` field is the layout of a `Vector2`,
        /// and so on). The library does not pad struct fields; the
        /// pool is packed.
        std::vector<char> struct_value_pool;

        /// The number of bytes at the start of `string_pool` that are
        /// the graph's own string literals.
        ///
        /// ─── The invariant ────────────────────────────────────────────
        /// Every `Literal::String` in this graph has an (offset, length)
        /// whose end (`offset + length`) is at most `literal_pool_size`.
        /// The bytes before `literal_pool_size` are the graph's
        /// literals; nothing else lives there.
        ///
        /// ─── Who sets it ──────────────────────────────────────────────
        /// `GraphBuilder::build` sets it after the string pool is fully
        /// populated. A graph produced by `compile` always has it set
        /// correctly.
        ///
        /// ─── Who reads it ─────────────────────────────────────────────
        /// `serialize` reads it to know where the graph's literal
        /// prefix ends. Bytes after the prefix are names that
        /// serialization interns; on the next `serialize` of the same
        /// graph, those names are appended again, starting from the
        /// prefix.
        ///
        /// ─── Hand-built graphs ────────────────────────────────────────
        /// A graph constructed by hand (in a test, in a tool) must set
        /// this field to the maximum extent of its string literals. If
        /// the graph has no string literals, the correct value is 0.
        /// Forgetting it produces a file whose string pool does not
        /// contain the graph's literal bytes, which corrupts any
        /// literal's text on the round-trip.
        uint32_t literal_pool_size = 0;

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