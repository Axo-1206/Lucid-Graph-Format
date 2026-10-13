/// @file tests/serialization/test_struct_roundtrip.cpp
///
/// @brief Round-trip tests for struct-valued resource fields.

#include "serialization/Serializer.hpp"
#include "serialization/Deserializer.hpp"
#include "serialization/BinaryFormat.hpp"

#include "sema/Graph.hpp"
#include "sema/Literal.hpp"
#include "sema/Registry.hpp"
#include "sema/TypeId.hpp"
#include "core/memory/ArenaSpan.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lucid;
using namespace lucid::serialization;

namespace
{

    const sema::Registry &testRegistry()
    {
        using namespace lucid::sema;

        static const StructFieldInfo vec2Fields[] = {
            {"x", TypeId::primitive("float32")},
            {"y", TypeId::primitive("float32")},
        };
        static const StructTypeInfo structs[] = {
            {"Vector2", ArenaSpan<StructFieldInfo>(vec2Fields, 2)},
        };

        static const Registry reg = []
        {
            Registry r;
            r.structs = ArenaSpan<StructTypeInfo>(structs, 1);
            return r;
        }();

        return reg;
    }

} // namespace

TEST_CASE("roundtrip: an empty struct pool",
          "[serialization][struct]")
{
    sema::Graph g;
    g.registry_fingerprint = sema::computeRegistryFingerprint(testRegistry());

    const std::vector<uint8_t> bytes = serialize(g);
    auto dr = deserialize(ByteSpan(bytes), testRegistry());

    REQUIRE(dr.ok);
    CHECK(dr.graph->struct_value_pool.empty());
}

TEST_CASE("roundtrip: a struct-valued resource field",
          "[serialization][struct]")
{
    sema::Graph g;
    g.registry_fingerprint = sema::computeRegistryFingerprint(testRegistry());

    // Build a Vector2 zero value (8 bytes of zeros) in the struct pool.
    g.struct_value_pool.resize(8, 0);

    sema::Resource r;
    r.name = "Config";
    r.fields_offset = 0;
    r.fields_count = 1;
    g.resources.push_back(r);

    sema::ResourceField f;
    f.name = "origin";
    f.type = sema::TypeId::structType("Vector2");
    f.value = sema::makeStruct(0, 8);
    g.resource_fields.push_back(f);

    const std::vector<uint8_t> bytes = serialize(g);
    auto dr = deserialize(ByteSpan(bytes), testRegistry());

    REQUIRE(dr.ok);
    REQUIRE(dr.graph != nullptr);

    // The struct pool survives.
    REQUIRE(dr.graph->struct_value_pool.size() == 8);
    for (char c : dr.graph->struct_value_pool)
        CHECK(c == 0);

    // The resource field's type and value survive.
    REQUIRE(dr.graph->resource_fields.size() == 1);
    const auto &rf = dr.graph->resource_fields[0];
    CHECK(rf.name == "origin");
    CHECK(rf.type.kind == sema::TypeId::Kind::Struct);
    CHECK(rf.type.name == "Vector2");
    CHECK(rf.value.kind == sema::Literal::Kind::Struct);
    CHECK(rf.value.struct_value.offset == 0);
    CHECK(rf.value.struct_value.length == 8);
}

TEST_CASE("roundtrip: a struct with a non-zero float field",
          "[serialization][struct]")
{
    sema::Graph g;
    g.registry_fingerprint = sema::computeRegistryFingerprint(testRegistry());

    // A Vector2 with x = 1.5, y = -2.5, laid out as two little-endian
    // float32s.
    const float x = 1.5f;
    const float y = -2.5f;
    g.struct_value_pool.resize(8);
    std::memcpy(g.struct_value_pool.data(), &x, 4);
    std::memcpy(g.struct_value_pool.data() + 4, &y, 4);

    sema::Resource r;
    r.name = "Config";
    r.fields_offset = 0;
    r.fields_count = 1;
    g.resources.push_back(r);

    sema::ResourceField f;
    f.name = "position";
    f.type = sema::TypeId::structType("Vector2");
    f.value = sema::makeStruct(0, 8);
    g.resource_fields.push_back(f);

    const std::vector<uint8_t> bytes = serialize(g);
    auto dr = deserialize(ByteSpan(bytes), testRegistry());

    REQUIRE(dr.ok);
    REQUIRE(dr.graph->struct_value_pool.size() == 8);

    float rx = 0.0f, ry = 0.0f;
    std::memcpy(&rx, dr.graph->struct_value_pool.data(), 4);
    std::memcpy(&ry, dr.graph->struct_value_pool.data() + 4, 4);
    CHECK(rx == 1.5f);
    CHECK(ry == -2.5f);
}

TEST_CASE("roundtrip: serialize after deserialize produces identical bytes",
          "[serialization][struct]")
{
    sema::Graph g;
    g.registry_fingerprint = sema::computeRegistryFingerprint(testRegistry());

    g.struct_value_pool.resize(8);
    sema::Resource r;
    r.name = "Config";
    g.resources.push_back(r);
    sema::ResourceField f;
    f.name = "origin";
    f.type = sema::TypeId::structType("Vector2");
    f.value = sema::makeStruct(0, 8);
    g.resource_fields.push_back(f);

    const std::vector<uint8_t> first = serialize(g);
    auto dr = deserialize(ByteSpan(first), testRegistry());
    REQUIRE(dr.ok);

    const std::vector<uint8_t> second = serialize(*dr.graph);
    CHECK(first == second);
}