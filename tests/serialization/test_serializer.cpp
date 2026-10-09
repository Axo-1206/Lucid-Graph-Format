/// @file tests/serialization/test_serializer.cpp
///
/// @brief Unit tests for serialize().
///
/// ─── What this tests ──────────────────────────────────────────────────────
/// The byte-level structure of the output: header, section table, each
/// section's count and record layout. It does not test round-tripping
/// (that needs deserialize, which arrives in Step 8.3).
///
/// ─── The empty-graph file size ────────────────────────────────────────────
/// Header:        32
/// Section table: 8 x 12 = 96
/// Section counts: 8 x 4 = 32
/// Total:         160 bytes
///
/// Every section is present, even when empty. The test asserts this
/// exact size.

#include "serialization/Serializer.hpp"
#include "serialization/BinaryFormat.hpp"

#include "sema/Graph.hpp"
#include "sema/Literal.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

using namespace lucid;
using namespace lucid::serialization;
using namespace lucid::serialization::format;

namespace
{

    // ─── Little-endian readers for the test ────────────────────────────

    uint16_t readU16(const std::vector<uint8_t> &v, size_t off)
    {
        return static_cast<uint16_t>(v[off]) | static_cast<uint16_t>(v[off + 1]) << 8;
    }

    uint32_t readU32(const std::vector<uint8_t> &v, size_t off)
    {
        return static_cast<uint32_t>(v[off]) | static_cast<uint32_t>(v[off + 1]) << 8 | static_cast<uint32_t>(v[off + 2]) << 16 | static_cast<uint32_t>(v[off + 3]) << 24;
    }

    uint64_t readU64(const std::vector<uint8_t> &v, size_t off)
    {
        return static_cast<uint64_t>(readU32(v, off)) | static_cast<uint64_t>(readU32(v, off + 4)) << 32;
    }

    /// Read the section-table entry at index `i`. The table starts at
    /// offset 32.
    struct SectionEntry
    {
        uint32_t id;
        uint32_t size;
    };

    SectionEntry readSectionEntry(const std::vector<uint8_t> &v, size_t i)
    {
        const size_t off = kHeaderSize + i * kSectionTableEntrySize;
        return SectionEntry{readU32(v, off), readU32(v, off + 4)};
    }

    /// The byte offset of the first section's data.
    constexpr size_t firstSectionOffset()
    {
        return kHeaderSize + kSectionTableEntrySize * kSectionCount;
    }

    /// The byte offset of the Nth section's data, given the sizes of
    /// the previous sections.
    size_t sectionOffset(const std::vector<uint8_t> &v, size_t i)
    {
        size_t off = firstSectionOffset();
        for (size_t k = 0; k < i; ++k)
        {
            off += readSectionEntry(v, k).size;
        }
        return off;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("serialize: empty graph is exactly 160 bytes",
          "[serialization][serializer]")
{
    sema::Graph g;
    const std::vector<uint8_t> bytes = serialize(g);

    CHECK(bytes.size() == 160);
}

TEST_CASE("serialize: header has the correct magic and version",
          "[serialization][serializer]")
{
    sema::Graph g;
    const std::vector<uint8_t> bytes = serialize(g);

    REQUIRE(bytes.size() >= kHeaderSize);

    CHECK(bytes[0] == 0x4C); // 'L'
    CHECK(bytes[1] == 0x55); // 'U'
    CHECK(bytes[2] == 0x47); // 'G'
    CHECK(bytes[3] == 0x52); // 'R'

    CHECK(readU16(bytes, 4) == kFormatVersion);
    CHECK(readU16(bytes, 6) == 0); // reserved

    CHECK(readU32(bytes, 16) == kSectionCount);
}

TEST_CASE("serialize: header writes the registry fingerprint",
          "[serialization][serializer]")
{
    sema::Graph g;
    g.registry_fingerprint = 0x0123456789ABCDEFULL;

    const std::vector<uint8_t> bytes = serialize(g);
    REQUIRE(bytes.size() >= kHeaderSize);

    CHECK(readU64(bytes, 8) == 0x0123456789ABCDEFULL);
}

TEST_CASE("serialize: section table lists all eight sections in order",
          "[serialization][serializer]")
{
    sema::Graph g;
    const std::vector<uint8_t> bytes = serialize(g);

    const uint32_t expectedIds[] = {
        static_cast<uint32_t>(SectionKind::Nodes),
        static_cast<uint32_t>(SectionKind::Args),
        static_cast<uint32_t>(SectionKind::Resources),
        static_cast<uint32_t>(SectionKind::ResourceFields),
        static_cast<uint32_t>(SectionKind::Subscribers),
        static_cast<uint32_t>(SectionKind::PhaseOrder),
        static_cast<uint32_t>(SectionKind::ValueOrder),
        static_cast<uint32_t>(SectionKind::StringPool),
    };

    REQUIRE(bytes.size() >= firstSectionOffset());
    for (size_t i = 0; i < 8; ++i)
    {
        const SectionEntry e = readSectionEntry(bytes, i);
        CHECK(e.id == expectedIds[i]);
        CHECK(e.size == 4); // count field only, no items
    }
}

TEST_CASE("serialize: empty sections hold a zero count",
          "[serialization][serializer]")
{
    sema::Graph g;
    const std::vector<uint8_t> bytes = serialize(g);

    for (size_t i = 0; i < 8; ++i)
    {
        const size_t off = sectionOffset(bytes, i);
        REQUIRE(off + 4 <= bytes.size());
        CHECK(readU32(bytes, off) == 0);
    }
}

TEST_CASE("serialize: one-node graph has the expected Node section shape",
          "[serialization][serializer]")
{
    sema::Graph g;
    sema::NodeInstance n;
    n.type_id = 3;
    n.phase = 0;
    n.args_offset = 0;
    n.args_count = 0;
    n.subscribers_offset = 0;
    n.subscribers_count = 0;
    g.nodes.push_back(n);

    const std::vector<uint8_t> bytes = serialize(g);

    // Node section: 4 bytes count + 24 bytes record = 28 bytes.
    const SectionEntry nodes = readSectionEntry(bytes, 0);
    CHECK(nodes.id == static_cast<uint32_t>(SectionKind::Nodes));
    CHECK(nodes.size == 4 + kNodeRecordSize);

    const size_t off = sectionOffset(bytes, 0);
    CHECK(readU32(bytes, off + 0) == 1);  // count
    CHECK(readU32(bytes, off + 4) == 3);  // type_id
    CHECK(readU32(bytes, off + 8) == 0);  // phase
    CHECK(readU32(bytes, off + 12) == 0); // args_offset
    CHECK(readU32(bytes, off + 16) == 0); // args_count
    CHECK(readU32(bytes, off + 20) == 0); // subscribers_offset
    CHECK(readU32(bytes, off + 24) == 0); // subscribers_count
}

TEST_CASE("serialize: a literal arg is written as kind + literal kind + payload",
          "[serialization][serializer]")
{
    sema::Graph g;

    sema::Literal lit;
    lit.kind = sema::Literal::Kind::Float32;
    lit.f = 1.5;

    g.args.push_back(sema::Arg::makeLiteral(lit));

    const std::vector<uint8_t> bytes = serialize(g);

    // Args section: 4 bytes count + 1 byte arg kind + 1 byte literal
    // kind + 4 bytes float payload = 10 bytes.
    const SectionEntry args = readSectionEntry(bytes, 1);
    CHECK(args.size == 4 + kLiteralArgPrefix + 4);

    const size_t off = sectionOffset(bytes, 1);
    CHECK(readU32(bytes, off + 0) == 1); // count
    CHECK(bytes[off + 4] == static_cast<uint8_t>(ArgKind::Literal));
    CHECK(bytes[off + 5] == static_cast<uint8_t>(LiteralKind::Float32));

    // The float's bits, little-endian.
    const uint32_t bits = readU32(bytes, off + 6);
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(f));
    CHECK(f == 1.5f);
}

TEST_CASE("serialize: a NodeRef arg is 5 bytes",
          "[serialization][serializer]")
{
    sema::Graph g;
    g.args.push_back(sema::Arg::makeNodeRef(7));

    const std::vector<uint8_t> bytes = serialize(g);

    const SectionEntry args = readSectionEntry(bytes, 1);
    CHECK(args.size == 4 + kNodeRefArgSize);

    const size_t off = sectionOffset(bytes, 1);
    CHECK(readU32(bytes, off + 0) == 1); // count
    CHECK(bytes[off + 4] == static_cast<uint8_t>(ArgKind::NodeRef));
    CHECK(readU32(bytes, off + 5) == 7);
}

TEST_CASE("serialize: a ResourceRef arg is 9 bytes",
          "[serialization][serializer]")
{
    sema::Graph g;
    g.args.push_back(sema::Arg::makeResourceRef(2, 5));

    const std::vector<uint8_t> bytes = serialize(g);

    const SectionEntry args = readSectionEntry(bytes, 1);
    CHECK(args.size == 4 + kResourceRefArgSize);

    const size_t off = sectionOffset(bytes, 1);
    CHECK(readU32(bytes, off + 0) == 1);
    CHECK(bytes[off + 4] == static_cast<uint8_t>(ArgKind::ResourceRef));
    CHECK(readU32(bytes, off + 5) == 2); // resource index
    CHECK(readU32(bytes, off + 9) == 5); // field index
}

TEST_CASE("serialize: a resource name is written as (offset, length) into the pool",
          "[serialization][serializer]")
{
    sema::Graph g;

    sema::Resource r;
    r.name = "PlayerConfig";
    r.fields_offset = 0;
    r.fields_count = 0;
    g.resources.push_back(r);

    const std::vector<uint8_t> bytes = serialize(g);

    // Resources section: 4 count + 16 record = 20 bytes.
    const SectionEntry res = readSectionEntry(bytes, 2);
    CHECK(res.size == 4 + kResourceRecordSize);

    const size_t off = sectionOffset(bytes, 2);
    CHECK(readU32(bytes, off + 0) == 1); // count
    const uint32_t nameOff = readU32(bytes, off + 4);
    const uint32_t nameLen = readU32(bytes, off + 8);
    CHECK(nameLen == 12); // "PlayerConfig"

    // The String Pool section contains "PlayerConfig" at nameOff.
    const size_t poolOff = sectionOffset(bytes, 7);
    const uint32_t poolSize = readU32(bytes, poolOff);
    CHECK(poolSize == 12); // only "PlayerConfig"
    REQUIRE(nameOff + nameLen <= poolSize);
    const std::string pool(reinterpret_cast<const char *>(&bytes[poolOff + 4]),
                           poolSize);
    CHECK(pool.substr(nameOff, nameLen) == "PlayerConfig");
}

TEST_CASE("serialize: distinct names share the pool; duplicates do not double-write",
          "[serialization][serializer]")
{
    sema::Graph g;

    sema::Resource r1;
    r1.name = "R";
    g.resources.push_back(r1);
    sema::Resource r2;
    r2.name = "R";
    g.resources.push_back(r2);

    const std::vector<uint8_t> bytes = serialize(g);

    // The pool should contain "R" once, not twice.
    const size_t poolOff = sectionOffset(bytes, 7);
    const uint32_t poolSize = readU32(bytes, poolOff);
    CHECK(poolSize == 1);

    // Both resources point at offset 0.
    const size_t resOff = sectionOffset(bytes, 2);
    const uint32_t nameOff0 = readU32(bytes, resOff + 4);
    const uint32_t nameOff1 = readU32(bytes, resOff + 20);
    CHECK(nameOff0 == 0);
    CHECK(nameOff1 == 0);
}

TEST_CASE("serialize: is deterministic",
          "[serialization][serializer]")
{
    sema::Graph g;
    sema::NodeInstance n;
    n.type_id = 0;
    n.phase = 0;
    g.nodes.push_back(n);
    g.args.push_back(sema::Arg::makeNodeRef(0));
    sema::Resource r;
    r.name = "R";
    g.resources.push_back(r);

    const std::vector<uint8_t> a = serialize(g);
    const std::vector<uint8_t> b = serialize(g);
    CHECK(a == b);
}

TEST_CASE("serialize: literal string offsets are preserved in the pool",
          "[serialization][serializer]")
{
    // The graph's own string_pool holds a literal's bytes at a known
    // offset. Serialize copies those bytes into the file's pool at
    // the same offset, so the literal's (offset, length) in the Args
    // section refers to the correct slice.

    sema::Graph g;
    g.string_pool = {'h', 'e', 'l', 'l', 'o'}; // "hello"
    g.literal_pool_size = 5;

    sema::Literal lit;
    lit.kind = sema::Literal::Kind::String;
    lit.string.offset = 0;
    lit.string.length = 5;
    g.args.push_back(sema::Arg::makeLiteral(lit));

    const std::vector<uint8_t> bytes = serialize(g);

    const size_t poolOff = sectionOffset(bytes, 7);
    const uint32_t poolSize = readU32(bytes, poolOff);
    CHECK(poolSize == 5);
    REQUIRE(poolOff + 4 + 5 <= bytes.size());
    CHECK(bytes[poolOff + 4 + 0] == 'h');
    CHECK(bytes[poolOff + 4 + 4] == 'o');
}