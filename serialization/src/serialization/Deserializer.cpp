/// @file serialization/src/serialization/Deserializer.cpp
///
/// @brief Implementation of deserialize().

#include "serialization/Deserializer.hpp"

#include "serialization/BinaryFormat.hpp"
#include "Reader.hpp"

#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/StringPool.hpp"
#include "sema/Literal.hpp"
#include "sema/Registry.hpp"
#include "sema/TypeId.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace lucid::serialization
{

    using namespace lucid::serialization::format;
    using lucid::sema::Arg;
    using lucid::sema::Graph;
    using lucid::sema::Literal;
    using lucid::sema::NodeInstance;
    using lucid::sema::Registry;
    using lucid::sema::Resource;
    using lucid::sema::ResourceField;
    using lucid::sema::TypeId;

    namespace
    {

        // ─── Section table entry, in-memory ────────────────────────────────

        struct SectionRef
        {
            SectionKind kind;
            size_t offset; // byte offset of the section's data
            size_t size;   // section size in bytes
        };

        // ─── Helpers to build a diagnostic ─────────────────────────────────

        lucid::diag::Diagnostic makeError(lucid::diag::DiagCode code,
                                          std::string message)
        {
            lucid::diag::Diagnostic d;
            d.code = code;
            d.severity = lucid::diag::Severity::Error;
            d.message = std::move(message);
            return d;
        }

        // ─── Literal reading ───────────────────────────────────────────────

        LiteralKind wireLiteralKindToInMemory(uint8_t wire) noexcept
        {
            switch (wire)
            {
            case 0:
                return LiteralKind::Nil;
            case 1:
                return LiteralKind::Bool;
            case 2:
                return LiteralKind::Char;
            case 3:
                return LiteralKind::String;
            case 4:
                return LiteralKind::Int8;
            case 5:
                return LiteralKind::Int16;
            case 6:
                return LiteralKind::Int32;
            case 7:
                return LiteralKind::Int64;
            case 8:
                return LiteralKind::UInt8;
            case 9:
                return LiteralKind::UInt16;
            case 10:
                return LiteralKind::UInt32;
            case 11:
                return LiteralKind::UInt64;
            case 12:
                return LiteralKind::Float32;
            case 13:
                return LiteralKind::Float64;
            default:
                return LiteralKind::Nil;
            }
        }

        Literal::Kind wireToInMemory(LiteralKind wire) noexcept
        {
            switch (wire)
            {
            case LiteralKind::Nil:
                return Literal::Kind::Nil;
            case LiteralKind::Bool:
                return Literal::Kind::Bool;
            case LiteralKind::Char:
                return Literal::Kind::Char;
            case LiteralKind::String:
                return Literal::Kind::String;
            case LiteralKind::Int8:
                return Literal::Kind::Int8;
            case LiteralKind::Int16:
                return Literal::Kind::Int16;
            case LiteralKind::Int32:
                return Literal::Kind::Int32;
            case LiteralKind::Int64:
                return Literal::Kind::Int64;
            case LiteralKind::UInt8:
                return Literal::Kind::UInt8;
            case LiteralKind::UInt16:
                return Literal::Kind::UInt16;
            case LiteralKind::UInt32:
                return Literal::Kind::UInt32;
            case LiteralKind::UInt64:
                return Literal::Kind::UInt64;
            case LiteralKind::Float32:
                return Literal::Kind::Float32;
            case LiteralKind::Float64:
                return Literal::Kind::Float64;
            }
            return Literal::Kind::Nil;
        }

        /// Read a literal-kind byte and the payload that follows.
        /// Returns the Literal. Advances the reader.
        Literal readLiteralPayload(detail::Reader &r)
        {
            Literal lit;
            const uint8_t kindByte = r.u8();
            const LiteralKind wire = wireLiteralKindToInMemory(kindByte);
            lit.kind = wireToInMemory(wire);

            switch (lit.kind)
            {
            case Literal::Kind::Nil:
                break;
            case Literal::Kind::Bool:
                lit.b = (r.u8() != 0);
                break;
            case Literal::Kind::Char:
                lit.c = static_cast<char>(r.u8());
                break;
            case Literal::Kind::String:
                lit.string.offset = r.u32();
                lit.string.length = r.u32();
                break;
            case Literal::Kind::Int8:
                lit.i = r.i8();
                break;
            case Literal::Kind::Int16:
                lit.i = r.i16();
                break;
            case Literal::Kind::Int32:
                lit.i = r.i32();
                break;
            case Literal::Kind::Int64:
                lit.i = r.i64();
                break;
            case Literal::Kind::UInt8:
                lit.u = r.u8();
                break;
            case Literal::Kind::UInt16:
                lit.u = r.u16();
                break;
            case Literal::Kind::UInt32:
                lit.u = r.u32();
                break;
            case Literal::Kind::UInt64:
                lit.u = r.u64();
                break;
            case Literal::Kind::Float32:
                lit.f = r.f32();
                break;
            case Literal::Kind::Float64:
                lit.f = r.f64();
                break;
            }
            return lit;
        }

        // ─── Section readers ───────────────────────────────────────────────
        //
        // Each reader takes a Reader positioned at the section's first
        // byte (the count field) and fills the corresponding field of
        // the graph.
        //
        // Sections that reference the string pool (Resources, Resource
        // Fields) are read *after* the String Pool section is read.

        bool readNodesSection(detail::Reader &r, Graph &g,
                              std::vector<lucid::diag::Diagnostic> &diags)
        {
            const uint32_t count = r.u32();
            if (r.overflowed())
            {
                diags.push_back(makeError(
                    lucid::diag::DiagCode::Ser_TruncatedSection,
                    "Nodes section is truncated"));
                return false;
            }

            g.nodes.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                NodeInstance n;
                n.type_id = r.u32();
                n.phase = r.u32();
                n.args_offset = r.u32();
                n.args_count = r.u32();
                n.subscribers_offset = r.u32();
                n.subscribers_count = r.u32();

                if (r.overflowed())
                {
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_TruncatedSection,
                        "Nodes section is truncated"));
                    return false;
                }
                g.nodes.push_back(n);
            }
            return true;
        }

        bool readArgsSection(detail::Reader &r, Graph &g,
                             std::vector<lucid::diag::Diagnostic> &diags)
        {
            const uint32_t count = r.u32();
            if (r.overflowed())
            {
                diags.push_back(makeError(
                    lucid::diag::DiagCode::Ser_TruncatedSection,
                    "Args section is truncated"));
                return false;
            }

            g.args.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                const uint8_t kindByte = r.u8();
                if (r.overflowed())
                {
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_TruncatedSection,
                        "Args section is truncated"));
                    return false;
                }

                Arg a;
                switch (static_cast<ArgKind>(kindByte))
                {
                case ArgKind::Literal:
                    a = Arg::makeLiteral(readLiteralPayload(r));
                    break;
                case ArgKind::NodeRef:
                    a = Arg::makeNodeRef(r.u32());
                    break;
                case ArgKind::ResourceRef:
                {
                    const uint32_t res = r.u32();
                    const uint32_t fld = r.u32();
                    a = Arg::makeResourceRef(res, fld);
                    break;
                }
                default:
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_UnknownArgKind,
                        "Args section contains an unknown arg kind"));
                    return false;
                }

                if (r.overflowed())
                {
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_TruncatedSection,
                        "Args section is truncated"));
                    return false;
                }
                g.args.push_back(a);
            }
            return true;
        }

        bool readResourcesSection(detail::Reader &r, Graph &g,
                                  const std::vector<char> &pool,
                                  std::vector<lucid::diag::Diagnostic> &diags)
        {
            const uint32_t count = r.u32();
            if (r.overflowed())
            {
                diags.push_back(makeError(
                    lucid::diag::DiagCode::Ser_TruncatedSection,
                    "Resources section is truncated"));
                return false;
            }

            g.resources.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                const uint32_t nameOffset = r.u32();
                const uint32_t nameLength = r.u32();
                const uint32_t fieldsOffset = r.u32();
                const uint32_t fieldsCount = r.u32();

                if (r.overflowed())
                {
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_TruncatedSection,
                        "Resources section is truncated"));
                    return false;
                }

                if (nameOffset + nameLength > pool.size())
                {
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_BadStringRef,
                        "Resource name refers outside the string pool"));
                    return false;
                }

                Resource res;
                res.name.assign(pool.data() + nameOffset, nameLength);
                res.fields_offset = fieldsOffset;
                res.fields_count = fieldsCount;
                g.resources.push_back(std::move(res));
            }
            return true;
        }

        bool readResourceFieldsSection(
            detail::Reader &r, Graph &g,
            const std::vector<char> &pool,
            std::vector<lucid::diag::Diagnostic> &diags)
        {
            const uint32_t count = r.u32();
            if (r.overflowed())
            {
                diags.push_back(makeError(
                    lucid::diag::DiagCode::Ser_TruncatedSection,
                    "Resource Fields section is truncated"));
                return false;
            }

            g.resource_fields.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                const uint32_t nameOffset = r.u32();
                const uint32_t nameLength = r.u32();
                const uint8_t typeKindByte = r.u8();
                const uint32_t typeNameOffset = r.u32();
                const uint32_t typeNameLength = r.u32();
                const uint8_t hasDefaultByte = r.u8();

                if (r.overflowed())
                {
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_TruncatedSection,
                        "Resource Fields section is truncated"));
                    return false;
                }

                if (nameOffset + nameLength > pool.size() ||
                    typeNameOffset + typeNameLength > pool.size())
                {
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_BadStringRef,
                        "Resource field name or type name refers outside "
                        "the string pool"));
                    return false;
                }

                ResourceField f;
                f.name.assign(pool.data() + nameOffset, nameLength);

                // The TypeId constructor copies the string_view into
                // its owned std::string.
                TypeId::Kind typeKind;
                switch (static_cast<TypeKind>(typeKindByte))
                {
                case TypeKind::Invalid:
                    typeKind = TypeId::Kind::Invalid;
                    break;
                case TypeKind::Primitive:
                    typeKind = TypeId::Kind::Primitive;
                    break;
                case TypeKind::Enum:
                    typeKind = TypeId::Kind::Enum;
                    break;
                case TypeKind::Handle:
                    typeKind = TypeId::Kind::Handle;
                    break;
                default:
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_BadTypeKind,
                        "Resource field has an unknown type kind"));
                    return false;
                }

                f.type = TypeId{
                    typeKind,
                    std::string_view(pool.data() + typeNameOffset,
                                     typeNameLength)};

                f.hasDefault = (hasDefaultByte != 0);
                if (f.hasDefault)
                {
                    f.defaultValue = readLiteralPayload(r);
                    if (r.overflowed())
                    {
                        diags.push_back(makeError(
                            lucid::diag::DiagCode::Ser_TruncatedSection,
                            "Resource Fields section is truncated"));
                        return false;
                    }
                }

                g.resource_fields.push_back(std::move(f));
            }
            return true;
        }

        bool readSubscribersSection(detail::Reader &r, Graph &g,
                                    std::vector<lucid::diag::Diagnostic> &diags)
        {
            const uint32_t count = r.u32();
            if (r.overflowed())
            {
                diags.push_back(makeError(
                    lucid::diag::DiagCode::Ser_TruncatedSection,
                    "Subscribers section is truncated"));
                return false;
            }
            g.subscribers.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                g.subscribers.push_back(r.u32());
                if (r.overflowed())
                {
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_TruncatedSection,
                        "Subscribers section is truncated"));
                    return false;
                }
            }
            return true;
        }

        bool readPhaseOrderSection(detail::Reader &r, Graph &g,
                                   std::vector<lucid::diag::Diagnostic> &diags)
        {
            const uint32_t count = r.u32();
            if (r.overflowed())
            {
                diags.push_back(makeError(
                    lucid::diag::DiagCode::Ser_TruncatedSection,
                    "Phase Order section is truncated"));
                return false;
            }
            g.phase_order.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                g.phase_order.push_back(r.u32());
                if (r.overflowed())
                {
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_TruncatedSection,
                        "Phase Order section is truncated"));
                    return false;
                }
            }
            return true;
        }

        bool readValueOrderSection(detail::Reader &r, Graph &g,
                                   std::vector<lucid::diag::Diagnostic> &diags)
        {
            const uint32_t count = r.u32();
            if (r.overflowed())
            {
                diags.push_back(makeError(
                    lucid::diag::DiagCode::Ser_TruncatedSection,
                    "Value Order section is truncated"));
                return false;
            }
            g.value_order.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                g.value_order.push_back(r.u32());
                if (r.overflowed())
                {
                    diags.push_back(makeError(
                        lucid::diag::DiagCode::Ser_TruncatedSection,
                        "Value Order section is truncated"));
                    return false;
                }
            }
            return true;
        }

        bool readStringPoolSection(detail::Reader &r,
                                   std::vector<char> &out,
                                   std::vector<lucid::diag::Diagnostic> &diags)
        {
            const uint32_t size = r.u32();
            if (r.overflowed())
            {
                diags.push_back(makeError(
                    lucid::diag::DiagCode::Ser_TruncatedSection,
                    "String Pool section is truncated"));
                return false;
            }
            out.reserve(size);
            r.bytes(out, size);
            if (r.overflowed())
            {
                diags.push_back(makeError(
                    lucid::diag::DiagCode::Ser_TruncatedSection,
                    "String Pool section is truncated"));
                return false;
            }
            return true;
        }

    } // namespace

    // ─── Public entry point ───────────────────────────────────────────────────

    DeserializeResult deserialize(ByteSpan bytes, const Registry &registry)
    {
        DeserializeResult result;

        // ─── Header ────────────────────────────────────────────────────────
        if (bytes.size() < kHeaderSize)
        {
            result.diagnostics.push_back(makeError(
                lucid::diag::DiagCode::Ser_TooSmall,
                "File is smaller than the 32-byte header"));
            return result;
        }

        detail::Reader r(bytes);

        const uint8_t m0 = r.u8();
        const uint8_t m1 = r.u8();
        const uint8_t m2 = r.u8();
        const uint8_t m3 = r.u8();
        if (m0 != kMagic[0] || m1 != kMagic[1] ||
            m2 != kMagic[2] || m3 != kMagic[3])
        {
            result.diagnostics.push_back(makeError(
                lucid::diag::DiagCode::Ser_BadMagic,
                "File does not start with the .lucgraph magic 'LUGR'"));
            return result;
        }

        const uint16_t formatVersion = r.u16();
        if (formatVersion != kFormatVersion)
        {
            result.diagnostics.push_back(makeError(
                lucid::diag::DiagCode::Ser_BadVersion,
                "Unsupported .lucgraph format version"));
            return result;
        }

        (void)r.u16(); // reserved

        const uint64_t storedFingerprint = r.u64();
        const uint64_t currentFingerprint =
            lucid::sema::computeRegistryFingerprint(registry);
        if (storedFingerprint != currentFingerprint)
        {
            result.diagnostics.push_back(makeError(
                lucid::diag::DiagCode::Ser_FingerprintMismatch,
                "The file was compiled against a different registry"));
            return result;
        }

        const uint32_t sectionCount = r.u32();
        if (sectionCount != kSectionCount)
        {
            result.diagnostics.push_back(makeError(
                lucid::diag::DiagCode::Ser_BadSectionCount,
                "File has an unexpected number of sections"));
            return result;
        }

        // 12 bytes reserved2.
        (void)r.u32();
        (void)r.u32();
        (void)r.u32();

        // ─── Section table ─────────────────────────────────────────────────
        if (bytes.size() < kHeaderSize +
                               kSectionTableEntrySize * sectionCount)
        {
            result.diagnostics.push_back(makeError(
                lucid::diag::DiagCode::Ser_TooSmall,
                "File is smaller than its header plus section table"));
            return result;
        }

        std::vector<SectionRef> table;
        table.reserve(sectionCount);

        size_t dataOffset = kHeaderSize +
                            kSectionTableEntrySize * sectionCount;

        for (uint32_t i = 0; i < sectionCount; ++i)
        {
            const uint32_t id = r.u32();
            const uint32_t size = r.u32();
            (void)r.u32(); // reserved

            SectionRef ref;
            ref.kind = static_cast<SectionKind>(id);
            ref.offset = dataOffset;
            ref.size = size;

            if (dataOffset + size > bytes.size())
            {
                result.diagnostics.push_back(makeError(
                    lucid::diag::DiagCode::Ser_TruncatedSection,
                    "A section extends past the end of the file"));
                return result;
            }

            table.push_back(ref);
            dataOffset += size;
        }

        // ─── Locate each section by ID ─────────────────────────────────────
        //
        // The reader is order-independent: it looks up the section it
        // needs by ID, not by position. Version 1's writer emits the
        // sections in ID order, but the reader does not require it.

        auto findSection = [&](SectionKind k) -> const SectionRef *
        {
            for (const SectionRef &s : table)
                if (s.kind == k)
                    return &s;
            return nullptr;
        };

        const SectionRef *secStringPool = findSection(SectionKind::StringPool);
        const SectionRef *secResources = findSection(SectionKind::Resources);
        const SectionRef *secResourceFields =
            findSection(SectionKind::ResourceFields);
        const SectionRef *secNodes = findSection(SectionKind::Nodes);
        const SectionRef *secArgs = findSection(SectionKind::Args);
        const SectionRef *secSubscribers =
            findSection(SectionKind::Subscribers);
        const SectionRef *secPhaseOrder = findSection(SectionKind::PhaseOrder);
        const SectionRef *secValueOrder = findSection(SectionKind::ValueOrder);

        if (secStringPool == nullptr || secResources == nullptr ||
            secResourceFields == nullptr || secNodes == nullptr ||
            secArgs == nullptr || secSubscribers == nullptr ||
            secPhaseOrder == nullptr || secValueOrder == nullptr)
        {
            result.diagnostics.push_back(makeError(
                lucid::diag::DiagCode::Ser_MissingSection,
                "File is missing a required section"));
            return result;
        }

        // ─── Build the graph ───────────────────────────────────────────────
        auto g = std::make_unique<Graph>();
        g->registry_fingerprint = storedFingerprint;

        // ─── 1. String Pool ────────────────────────────────────────────────
        {
            detail::Reader sr(bytes.subspan(secStringPool->offset,
                                            secStringPool->size));
            if (!readStringPoolSection(sr, g->string_pool, result.diagnostics))
                return result;
        }

        // ─── 2. Resources (needs the pool) ─────────────────────────────────
        {
            detail::Reader sr(bytes.subspan(secResources->offset,
                                            secResources->size));
            if (!readResourcesSection(sr, *g, g->string_pool,
                                      result.diagnostics))
                return result;
        }

        // ─── 3. Resource Fields (needs the pool) ───────────────────────────
        {
            detail::Reader sr(bytes.subspan(secResourceFields->offset,
                                            secResourceFields->size));
            if (!readResourceFieldsSection(sr, *g, g->string_pool,
                                           result.diagnostics))
                return result;
        }

        // ─── 4. Nodes ──────────────────────────────────────────────────────
        {
            detail::Reader sr(bytes.subspan(secNodes->offset,
                                            secNodes->size));
            if (!readNodesSection(sr, *g, result.diagnostics))
                return result;
        }

        // ─── 5. Args ───────────────────────────────────────────────────────
        {
            detail::Reader sr(bytes.subspan(secArgs->offset,
                                            secArgs->size));
            if (!readArgsSection(sr, *g, result.diagnostics))
                return result;
        }

        // ─── 6. Subscribers ────────────────────────────────────────────────
        {
            detail::Reader sr(bytes.subspan(secSubscribers->offset,
                                            secSubscribers->size));
            if (!readSubscribersSection(sr, *g, result.diagnostics))
                return result;
        }

        // ─── 7. Phase Order ────────────────────────────────────────────────
        {
            detail::Reader sr(bytes.subspan(secPhaseOrder->offset,
                                            secPhaseOrder->size));
            if (!readPhaseOrderSection(sr, *g, result.diagnostics))
                return result;
        }

        // ─── 8. Value Order ────────────────────────────────────────────────
        {
            detail::Reader sr(bytes.subspan(secValueOrder->offset,
                                            secValueOrder->size));
            if (!readValueOrderSection(sr, *g, result.diagnostics))
                return result;
        }

        // ─── 9. compute literal_pool_size ───────────────────────────────────

        // The file does not record where the literal prefix ends; it
        // only records the pool's bytes and the literals' (offset,
        // length) into it. The deserializer recovers the prefix from
        // the literals themselves: the prefix is the maximum
        // (offset + length) over every string literal in the graph.
        //
        // Why this is exact: the serializer wrote the file's pool
        // with the graph's literal prefix first, then the interned
        // names. No literal's bytes appear after the prefix, so the
        // max extent of the literals is the prefix's end. On a graph
        // with no string literals, the max extent is 0, and the
        // prefix is empty — correct.
        //
        // This makes serialize → deserialize → serialize produce
        // byte-identical output: the second serialize starts the
        // file's pool from the recovered prefix, exactly as the
        // first one did.
        uint32_t literalEnd = 0;
        for (const Arg &a : g->args)
        {
            if (a.kind == Arg::Kind::Literal &&
                a.literal.kind == Literal::Kind::String)
            {
                const uint32_t e =
                    a.literal.string.offset + a.literal.string.length;
                if (e > literalEnd)
                    literalEnd = e;
            }
        }
        for (const ResourceField &f : g->resource_fields)
        {
            if (f.hasDefault &&
                f.defaultValue.kind == Literal::Kind::String)
            {
                const uint32_t e =
                    f.defaultValue.string.offset +
                    f.defaultValue.string.length;
                if (e > literalEnd)
                    literalEnd = e;
            }
        }
        g->literal_pool_size = literalEnd;

        // ─── Done ──────────────────────────────────────────────────────────
        result.ok = true;
        result.graph = std::move(g);
        return result;
    }

} // namespace lucid::serialization