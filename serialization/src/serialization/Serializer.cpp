/// @file serialization/src/serialization/Serializer.cpp
///
/// @brief Implementation of serialize(const Graph&).

#include "serialization/Serializer.hpp"

#include "serialization/BinaryFormat.hpp"
#include "Writer.hpp"

#include "sema/Literal.hpp"
#include "sema/TypeId.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lucid::serialization
{

    using namespace lucid::serialization::format;
    using lucid::sema::Arg;
    using lucid::sema::Graph;
    using lucid::sema::Literal;
    using lucid::sema::NodeInstance;
    using lucid::sema::Resource;
    using lucid::sema::ResourceField;
    using lucid::sema::TypeId;

    namespace
    {

        // ─── The string pool builder ───────────────────────────────────────
        //
        // A file-local string pool. Starts as a copy of the graph's
        // existing pool (which holds string literals), then accepts
        // additional names. Every name is assigned an (offset, length)
        // into the pool. A name repeated is not appended twice; the
        // first offset is reused.
        //
        // The graph itself is not modified.

        class FileStringPool
        {
        public:
            explicit FileStringPool(const std::vector<char> &graphPool)
                : m_pool(graphPool.begin(), graphPool.end())
            {
            }

            /// Append `s` (if not already present), return its offset.
            /// The returned offset is into the file's pool, which is
            /// the concatenation of the graph's literals and the
            /// appended names.
            uint32_t intern(std::string_view s)
            {
                const std::string key(s);
                auto it = m_index.find(key);
                if (it != m_index.end())
                {
                    return it->second;
                }

                const uint32_t offset =
                    static_cast<uint32_t>(m_pool.size());
                m_pool.insert(m_pool.end(), s.begin(), s.end());
                m_index.emplace(key, offset);
                return offset;
            }

            const std::vector<uint8_t> &bytes() const noexcept
            {
                return m_pool;
            }

        private:
            std::vector<uint8_t> m_pool;
            std::unordered_map<std::string, uint32_t> m_index;
        };

        // ─── Literal kind mapping ──────────────────────────────────────────
        //
        // The in-memory enum's ordering is an implementation detail;
        // the on-wire values are fixed by the format. This function
        // makes the mapping explicit so a future reorder of the
        // in-memory enum does not change the wire format.

        LiteralKind literalKindOf(Literal::Kind k) noexcept
        {
            switch (k)
            {
            case Literal::Kind::Nil:
                return LiteralKind::Nil;
            case Literal::Kind::Bool:
                return LiteralKind::Bool;
            case Literal::Kind::Char:
                return LiteralKind::Char;
            case Literal::Kind::String:
                return LiteralKind::String;
            case Literal::Kind::Int8:
                return LiteralKind::Int8;
            case Literal::Kind::Int16:
                return LiteralKind::Int16;
            case Literal::Kind::Int32:
                return LiteralKind::Int32;
            case Literal::Kind::Int64:
                return LiteralKind::Int64;
            case Literal::Kind::UInt8:
                return LiteralKind::UInt8;
            case Literal::Kind::UInt16:
                return LiteralKind::UInt16;
            case Literal::Kind::UInt32:
                return LiteralKind::UInt32;
            case Literal::Kind::UInt64:
                return LiteralKind::UInt64;
            case Literal::Kind::Float32:
                return LiteralKind::Float32;
            case Literal::Kind::Float64:
                return LiteralKind::Float64;
            }
            return LiteralKind::Nil;
        }

        TypeKind typeKindOf(TypeId::Kind k) noexcept
        {
            switch (k)
            {
            case TypeId::Kind::Invalid:
                return TypeKind::Invalid;
            case TypeId::Kind::Primitive:
                return TypeKind::Primitive;
            case TypeId::Kind::Enum:
                return TypeKind::Enum;
            case TypeId::Kind::Handle:
                return TypeKind::Handle;
            }
            return TypeKind::Invalid;
        }

        // ─── Literal payload ───────────────────────────────────────────────
        //
        // Writes the literal-kind byte followed by the payload. The
        // payload for String is (offset, length) into the file's pool;
        // every other kind is a fixed-width value.

        void writeLiteralPayload(detail::Writer &w, const Literal &lit)
        {
            w.u8(static_cast<uint8_t>(literalKindOf(lit.kind)));

            switch (lit.kind)
            {
            case Literal::Kind::Nil:
                // 0 bytes.
                break;
            case Literal::Kind::Bool:
                w.u8(lit.b ? 1 : 0);
                break;
            case Literal::Kind::Char:
                w.u8(static_cast<uint8_t>(lit.c));
                break;
            case Literal::Kind::String:
                w.u32(lit.string.offset);
                w.u32(lit.string.length);
                break;
            case Literal::Kind::Int8:
                w.i8(static_cast<int8_t>(lit.i));
                break;
            case Literal::Kind::Int16:
                w.i16(static_cast<int16_t>(lit.i));
                break;
            case Literal::Kind::Int32:
                w.i32(static_cast<int32_t>(lit.i));
                break;
            case Literal::Kind::Int64:
                w.i64(lit.i);
                break;
            case Literal::Kind::UInt8:
                w.u8(static_cast<uint8_t>(lit.u));
                break;
            case Literal::Kind::UInt16:
                w.u16(static_cast<uint16_t>(lit.u));
                break;
            case Literal::Kind::UInt32:
                w.u32(static_cast<uint32_t>(lit.u));
                break;
            case Literal::Kind::UInt64:
                w.u64(lit.u);
                break;
            case Literal::Kind::Float32:
                w.f32(static_cast<float>(lit.f));
                break;
            case Literal::Kind::Float64:
                w.f64(lit.f);
                break;
            }
        }

    } // namespace

    // ─── Public entry point ───────────────────────────────────────────────────

    std::vector<uint8_t> serialize(const Graph &graph)
    {
        // ─── Step 1: build the file's string pool ─────────────────────────
        //
        // Start with the graph's literal bytes. Then intern every name
        // the file will need: resource names, resource-field names, and
        // TypeId names. Record each name's offset in `nameOffsets` so
        // the section writers can look it up in O(1).
        //
        // The graph is not modified.

        FileStringPool pool(std::vector<char>(
            graph.string_pool.begin(),
            graph.string_pool.begin() + graph.literal_pool_size));

        // nameOffsets: name -> offset in the file's pool. Filled by
        // the helper below; kept here because the same name appears
        // in multiple records and we do not want to intern twice.

        struct NameOffset
        {
            uint32_t offset;
            uint32_t length;
        };

        auto internName = [&](std::string_view s) -> NameOffset
        {
            const uint32_t off = pool.intern(s);
            return NameOffset{off, static_cast<uint32_t>(s.size())};
        };

        std::vector<NameOffset> resourceNameOffsets;
        resourceNameOffsets.reserve(graph.resources.size());
        for (const Resource &r : graph.resources)
        {
            resourceNameOffsets.push_back(internName(r.name));
        }

        std::vector<NameOffset> fieldNameOffsets;
        fieldNameOffsets.reserve(graph.resource_fields.size());
        for (const ResourceField &f : graph.resource_fields)
        {
            fieldNameOffsets.push_back(internName(f.name));
        }

        std::vector<NameOffset> typeNameOffsets;
        typeNameOffsets.reserve(graph.resource_fields.size());
        for (const ResourceField &f : graph.resource_fields)
        {
            typeNameOffsets.push_back(internName(f.type.name));
        }

        // ─── Step 2: build each section's bytes ───────────────────────────
        //
        // Each section is built in its own vector first, then appended.
        // This lets us know each section's size before writing the
        // section table, which precedes the sections.

        // ─── Section: Nodes ────────────────────────────────────────────────
        std::vector<uint8_t> secNodes;
        {
            detail::Writer w(secNodes);
            w.u32(static_cast<uint32_t>(graph.nodes.size()));
            for (const NodeInstance &n : graph.nodes)
            {
                w.u32(n.type_id);
                w.u32(n.phase);
                w.u32(n.args_offset);
                w.u32(n.args_count);
                w.u32(n.subscribers_offset);
                w.u32(n.subscribers_count);
            }
        }

        // ─── Section: Args ─────────────────────────────────────────────────
        std::vector<uint8_t> secArgs;
        {
            detail::Writer w(secArgs);
            w.u32(static_cast<uint32_t>(graph.args.size()));
            for (const Arg &a : graph.args)
            {
                switch (a.kind)
                {
                case Arg::Kind::Literal:
                    w.u8(static_cast<uint8_t>(ArgKind::Literal));
                    writeLiteralPayload(w, a.literal);
                    break;
                case Arg::Kind::NodeRef:
                    w.u8(static_cast<uint8_t>(ArgKind::NodeRef));
                    w.u32(a.node_ref);
                    break;
                case Arg::Kind::ResourceRef:
                    w.u8(static_cast<uint8_t>(ArgKind::ResourceRef));
                    w.u32(a.resource_ref.resource_index);
                    w.u32(a.resource_ref.field_index);
                    break;
                }
            }
        }

        // ─── Section: Resources ────────────────────────────────────────────
        std::vector<uint8_t> secResources;
        {
            detail::Writer w(secResources);
            w.u32(static_cast<uint32_t>(graph.resources.size()));
            for (size_t i = 0; i < graph.resources.size(); ++i)
            {
                const Resource &r = graph.resources[i];
                const NameOffset &n = resourceNameOffsets[i];
                w.u32(n.offset);
                w.u32(n.length);
                w.u32(r.fields_offset);
                w.u32(r.fields_count);
            }
        }

        // ─── Section: Resource Fields ──────────────────────────────────────
        std::vector<uint8_t> secResourceFields;
        {
            detail::Writer w(secResourceFields);
            w.u32(static_cast<uint32_t>(graph.resource_fields.size()));
            for (size_t i = 0; i < graph.resource_fields.size(); ++i)
            {
                const ResourceField &f = graph.resource_fields[i];
                const NameOffset &name = fieldNameOffsets[i];
                const NameOffset &type = typeNameOffsets[i];

                w.u32(name.offset);
                w.u32(name.length);
                w.u8(static_cast<uint8_t>(typeKindOf(f.type.kind)));
                w.u32(type.offset);
                w.u32(type.length);
                w.u8(f.hasDefault ? 1 : 0);
                if (f.hasDefault)
                {
                    writeLiteralPayload(w, f.defaultValue);
                }
            }
        }

        // ─── Section: Subscribers ──────────────────────────────────────────
        std::vector<uint8_t> secSubscribers;
        {
            detail::Writer w(secSubscribers);
            w.u32(static_cast<uint32_t>(graph.subscribers.size()));
            for (uint32_t s : graph.subscribers)
            {
                w.u32(s);
            }
        }

        // ─── Section: Phase Order ──────────────────────────────────────────
        std::vector<uint8_t> secPhaseOrder;
        {
            detail::Writer w(secPhaseOrder);
            w.u32(static_cast<uint32_t>(graph.phase_order.size()));
            for (uint32_t n : graph.phase_order)
            {
                w.u32(n);
            }
        }

        // ─── Section: Value Order ──────────────────────────────────────────
        std::vector<uint8_t> secValueOrder;
        {
            detail::Writer w(secValueOrder);
            w.u32(static_cast<uint32_t>(graph.value_order.size()));
            for (uint32_t n : graph.value_order)
            {
                w.u32(n);
            }
        }

        // ─── Section: String Pool ──────────────────────────────────────────
        std::vector<uint8_t> secStringPool;
        {
            detail::Writer w(secStringPool);
            const auto &bytes = pool.bytes();
            w.u32(static_cast<uint32_t>(bytes.size()));
            w.bytes(bytes);
        }

        // ─── Step 3: assemble the file ────────────────────────────────────
        //
        // Layout: header, section table, sections.

        std::vector<uint8_t> out;
        out.reserve(
            format::kHeaderSize +
            format::kSectionTableEntrySize * format::kSectionCount +
            secNodes.size() + secArgs.size() + secResources.size() +
            secResourceFields.size() + secSubscribers.size() +
            secPhaseOrder.size() + secValueOrder.size() +
            secStringPool.size());

        detail::Writer w(out);

        // ─── Header (32 bytes) ─────────────────────────────────────────────
        w.u8(format::kMagic[0]);
        w.u8(format::kMagic[1]);
        w.u8(format::kMagic[2]);
        w.u8(format::kMagic[3]);
        w.u16(format::kFormatVersion);
        w.u16(0); // reserved
        w.u64(graph.registry_fingerprint);
        w.u32(format::kSectionCount);
        // 12 bytes reserved2
        w.u32(0);
        w.u32(0);
        w.u32(0);

        // ─── Section table (8 x 12 bytes) ──────────────────────────────────
        //
        // Each entry: section_id (4), section_size (4), reserved (4).
        // The sizes are known from step 2. The entries are written in
        // the fixed order the format defines.

        struct SectionEntry
        {
            SectionKind id;
            const std::vector<uint8_t> *bytes;
        };

        const SectionEntry sections[] = {
            {SectionKind::Nodes, &secNodes},
            {SectionKind::Args, &secArgs},
            {SectionKind::Resources, &secResources},
            {SectionKind::ResourceFields, &secResourceFields},
            {SectionKind::Subscribers, &secSubscribers},
            {SectionKind::PhaseOrder, &secPhaseOrder},
            {SectionKind::ValueOrder, &secValueOrder},
            {SectionKind::StringPool, &secStringPool},
        };

        for (const SectionEntry &s : sections)
        {
            w.u32(static_cast<uint32_t>(s.id));
            w.u32(static_cast<uint32_t>(s.bytes->size()));
            w.u32(0); // reserved
        }

        // ─── Section data ──────────────────────────────────────────────────
        for (const SectionEntry &s : sections)
        {
            w.bytes(*s.bytes);
        }

        return out;
    }

} // namespace lucid::serialization