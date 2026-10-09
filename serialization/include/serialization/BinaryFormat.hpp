/// @file serialization/BinaryFormat.hpp
///
/// @brief The byte-level constants of the .lucgraph format.
///
/// Every magic number, every fixed size, every enum value used on the
/// wire. The writer and the reader both include this header; neither
/// hard-codes a constant. A future format version bumps the version
/// number here.
///
/// ─── The format, in one paragraph ─────────────────────────────────────────
/// A .lucgraph file is a 32-byte header, followed by a section table of
/// N x 12 bytes, followed by N sections concatenated. Every multi-byte
/// integer is little-endian. Sections are packed with no alignment
/// padding. Each section begins with a 4-byte count of items (or, for
/// the String Pool, a 4-byte count of bytes). Every variable-length
/// datum is length-prefixed. The header carries the format version and
/// the registry fingerprint; the loader refuses a file whose version it
/// does not know or whose fingerprint does not match the current
/// registry.
///
/// ─── The string pool, on disk ─────────────────────────────────────────────
/// The String Pool section's bytes are the concatenation of two
/// regions, in order:
///
///   1. The graph's string literals. Their offsets in the file are
///      identical to their offsets in the in-memory graph, because
///      the graph's literal prefix is copied verbatim. Every
///      Literal::String's (offset, length) therefore needs no
///      remapping on write.
///   2. The graph's names — resource names, resource-field names, and
///      TypeId names. Each distinct name is interned once. Its
///      offset is recorded in the Resources and Resource Fields
///      sections.
///
/// The boundary between the two regions is not stored in the file.
/// The loader recovers it from the literals' extents (see
/// `Graph::literal_pool_size` in `sema/Graph.hpp`).

#pragma once

#include <cstddef>
#include <cstdint>

namespace lucid::serialization::format
{

    // ─── Header ───────────────────────────────────────────────────────────────

    /// The ASCII bytes "LUGR" (0x4C 0x55 0x47 0x52).
    inline constexpr uint8_t kMagic[4] = {0x4C, 0x55, 0x47, 0x52};

    /// The current format version. Bump when the byte layout changes.
    inline constexpr uint16_t kFormatVersion = 1;

    /// The number of bytes in the header.
    inline constexpr size_t kHeaderSize = 32;

    /// The number of bytes in one section-table entry.
    inline constexpr size_t kSectionTableEntrySize = 12;

    /// The number of bytes in one section's item count (or, for the
    /// String Pool, the byte count).
    inline constexpr size_t kSectionCountSize = 4;

    // ─── Section IDs ──────────────────────────────────────────────────────────

    enum class SectionKind : uint32_t
    {
        Nodes = 0x00000001,
        Args = 0x00000002,
        Resources = 0x00000003,
        ResourceFields = 0x00000004,
        Subscribers = 0x00000005,
        PhaseOrder = 0x00000006,
        ValueOrder = 0x00000007,
        StringPool = 0x00000008,
    };

    /// The number of sections version 1 defines.
    inline constexpr uint32_t kSectionCount = 8;

    // ─── Arg kinds ────────────────────────────────────────────────────────────

    enum class ArgKind : uint8_t
    {
        Literal = 0x01,
        NodeRef = 0x02,
        ResourceRef = 0x03,
    };

    // ─── Literal kinds ────────────────────────────────────────────────────────
    //
    // These are the on-wire values. They match sema::Literal::Kind's
    // ordering, but the mapping is written explicitly in the serializer
    // so that a change to the in-memory enum does not silently change
    // the on-wire encoding.

    enum class LiteralKind : uint8_t
    {
        Nil = 0,
        Bool = 1,
        Char = 2,
        String = 3,
        Int8 = 4,
        Int16 = 5,
        Int32 = 6,
        Int64 = 7,
        UInt8 = 8,
        UInt16 = 9,
        UInt32 = 10,
        UInt64 = 11,
        Float32 = 12,
        Float64 = 13,
    };

    // ─── TypeId kinds ─────────────────────────────────────────────────────────

    enum class TypeKind : uint8_t
    {
        Invalid = 0,
        Primitive = 1,
        Enum = 2,
        Handle = 3,
    };

    // ─── Fixed record sizes ───────────────────────────────────────────────────

    /// NodeInstance: 6 x uint32_t.
    inline constexpr size_t kNodeRecordSize = 24;

    /// Resource: name_offset, name_length, fields_offset, fields_count.
    inline constexpr size_t kResourceRecordSize = 16;

    /// ResourceField's fixed prefix (before an optional default):
    /// name_offset, name_length, type_kind, type_name_offset,
    /// type_name_length, has_default.
    inline constexpr size_t kResourceFieldFixedPrefix = 18;

    /// A NodeRef arg: 1 byte kind + 4 bytes index.
    inline constexpr size_t kNodeRefArgSize = 5;

    /// A ResourceRef arg: 1 byte kind + 4 bytes resource + 4 bytes field.
    inline constexpr size_t kResourceRefArgSize = 9;

    /// A Literal arg's prefix: 1 byte arg kind + 1 byte literal kind.
    /// The literal payload follows.
    inline constexpr size_t kLiteralArgPrefix = 2;

} // namespace lucid::serialization::format