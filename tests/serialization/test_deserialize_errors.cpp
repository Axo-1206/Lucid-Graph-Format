/// @file tests/serialization/test_deserialize_errors.cpp
///
/// @brief Error-path tests for deserialize().
///
/// ─── What this tests ──────────────────────────────────────────────────────
/// Each case constructs a malformed byte buffer and asserts that
/// deserialize refuses it with the correct diagnostic, and does not
/// crash or return a graph.

#include "serialization/Serializer.hpp"
#include "serialization/Deserializer.hpp"
#include "serialization/BinaryFormat.hpp"

#include "sema/Graph.hpp"
#include "sema/Registry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

using namespace lucid;
using namespace lucid::serialization;
using namespace lucid::serialization::format;

namespace
{

    const sema::Registry &testRegistry()
    {
        using namespace lucid::sema;

        static const NodeTypeInfo nodeTypes[] = {
            {"Float32Node", NodeKind::Value, "Math", 0, {}, TypeId::primitive("float32")},
        };

        static const Registry reg = []
        {
            Registry r;
            r.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes, 1);
            return r;
        }();

        return reg;
    }

    /// Serialize an empty graph with the test registry's fingerprint.
    std::vector<uint8_t> validEmptyFile()
    {
        sema::Graph g;
        g.registry_fingerprint = sema::computeRegistryFingerprint(testRegistry());
        return serialize(g);
    }

} // namespace

TEST_CASE("deserialize: too-small file is rejected",
          "[serialization][deserialize][errors]")
{
    std::vector<uint8_t> bytes(10, 0);
    DeserializeResult dr = deserialize(ByteSpan(bytes), testRegistry());
    CHECK_FALSE(dr.ok);
    CHECK(dr.graph == nullptr);
    REQUIRE_FALSE(dr.diagnostics.empty());
}

TEST_CASE("deserialize: bad magic is rejected",
          "[serialization][deserialize][errors]")
{
    std::vector<uint8_t> bytes = validEmptyFile();
    bytes[0] = 'X'; // corrupt the magic

    DeserializeResult dr = deserialize(ByteSpan(bytes), testRegistry());
    CHECK_FALSE(dr.ok);
    REQUIRE_FALSE(dr.diagnostics.empty());
    CHECK(dr.diagnostics[0].code == lucid::diag::DiagCode::Ser_BadMagic);
}

TEST_CASE("deserialize: wrong format version is rejected",
          "[serialization][deserialize][errors]")
{
    std::vector<uint8_t> bytes = validEmptyFile();
    bytes[4] = 99; // low byte of version
    bytes[5] = 0;  // high byte of version

    DeserializeResult dr = deserialize(ByteSpan(bytes), testRegistry());
    CHECK_FALSE(dr.ok);
    REQUIRE_FALSE(dr.diagnostics.empty());
    CHECK(dr.diagnostics[0].code == lucid::diag::DiagCode::Ser_BadVersion);
}

TEST_CASE("deserialize: fingerprint mismatch is rejected",
          "[serialization][deserialize][errors]")
{
    std::vector<uint8_t> bytes = validEmptyFile();
    // Corrupt one byte of the fingerprint (offset 8..15).
    bytes[8] ^= 0xFF;

    DeserializeResult dr = deserialize(ByteSpan(bytes), testRegistry());
    CHECK_FALSE(dr.ok);
    REQUIRE_FALSE(dr.diagnostics.empty());
    CHECK(dr.diagnostics[0].code ==
          lucid::diag::DiagCode::Ser_FingerprintMismatch);
}

TEST_CASE("deserialize: a truncated section is rejected",
          "[serialization][deserialize][errors]")
{
    std::vector<uint8_t> bytes = validEmptyFile();
    // Drop the last byte of the string pool section.
    bytes.pop_back();

    DeserializeResult dr = deserialize(ByteSpan(bytes), testRegistry());
    CHECK_FALSE(dr.ok);
    REQUIRE_FALSE(dr.diagnostics.empty());
}

TEST_CASE("deserialize: a section table entry that overruns the file "
          "is rejected",
          "[serialization][deserialize][errors]")
{
    std::vector<uint8_t> bytes = validEmptyFile();

    // The first section-table entry starts at offset 32. Its
    // section_size field is at offset 36. Set it to a huge value.
    bytes[36] = 0xFF;
    bytes[37] = 0xFF;
    bytes[38] = 0xFF;
    bytes[39] = 0x7F;

    DeserializeResult dr = deserialize(ByteSpan(bytes), testRegistry());
    CHECK_FALSE(dr.ok);
    REQUIRE_FALSE(dr.diagnostics.empty());
}