/// @file tests/serialization/test_roundtrip.cpp
///
/// @brief Round-trip: deserialize(serialize(g)) == g, structurally.
///
/// ─── What "structurally identical" means ──────────────────────────────────
/// Every vector matches element-wise. Every string matches byte-wise.
/// The fingerprint matches. The two graphs are equal as values.
///
/// The test does not compare memory addresses. The deserialized graph's
/// vectors are freshly allocated. That is the point of the format.

#include "serialization/Serializer.hpp"
#include "serialization/Deserializer.hpp"

#include "sema/Graph.hpp"
#include "sema/Literal.hpp"
#include "sema/Registry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace lucid;
using namespace lucid::serialization;

namespace
{

    /// A small registry used by the round-trip tests. Contains two node
    /// types so the graph's node type IDs are meaningful.
    const sema::Registry &testRegistry()
    {
        using namespace lucid::sema;

        static const NodeTypeInfo nodeTypes[] = {
            {"Float32Node", NodeKind::Value, "Math", 0, {}, TypeId::primitive("float32")},
            {"EveryFrame", NodeKind::Trigger, "Flow", 0, {}, TypeId{}},
        };

        static const Registry reg = []
        {
            Registry r;
            r.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes, 2);
            return r;
        }();

        return reg;
    }

    bool sameArgs(const sema::Arg &a, const sema::Arg &b)
    {
        if (a.kind != b.kind)
            return false;
        switch (a.kind)
        {
        case sema::Arg::Kind::Literal:
            return a.literal.kind == b.literal.kind &&
                   a.literal.i == b.literal.i &&
                   a.literal.u == b.literal.u &&
                   a.literal.f == b.literal.f &&
                   a.literal.string.offset == b.literal.string.offset &&
                   a.literal.string.length == b.literal.string.length;
        case sema::Arg::Kind::NodeRef:
            return a.node_ref == b.node_ref;
        case sema::Arg::Kind::ResourceRef:
            return a.resource_ref.resource_index ==
                       b.resource_ref.resource_index &&
                   a.resource_ref.field_index ==
                       b.resource_ref.field_index;
        }
        return false;
    }

    bool sameTypeId(const sema::TypeId &a, const sema::TypeId &b)
    {
        return a.kind == b.kind && a.name == b.name;
    }

} // namespace

TEST_CASE("roundtrip: empty graph",
          "[serialization][roundtrip]")
{
    sema::Graph g;
    g.registry_fingerprint = sema::computeRegistryFingerprint(testRegistry());

    const std::vector<uint8_t> bytes = serialize(g);
    DeserializeResult dr = deserialize(ByteSpan(bytes), testRegistry());

    REQUIRE(dr.ok);
    REQUIRE(dr.graph != nullptr);

    CHECK(dr.graph->nodes.empty());
    CHECK(dr.graph->args.empty());
    CHECK(dr.graph->resources.empty());
    CHECK(dr.graph->resource_fields.empty());
    CHECK(dr.graph->subscribers.empty());
    CHECK(dr.graph->phase_order.empty());
    CHECK(dr.graph->value_order.empty());
    CHECK(dr.graph->string_pool.empty());
    CHECK(dr.graph->registry_fingerprint ==
          sema::computeRegistryFingerprint(testRegistry()));
}

TEST_CASE("roundtrip: nodes and args",
          "[serialization][roundtrip]")
{
    sema::Graph g;
    g.registry_fingerprint = sema::computeRegistryFingerprint(testRegistry());

    sema::NodeInstance n0;
    n0.type_id = 0;
    n0.phase = 0;
    n0.args_offset = 0;
    n0.args_count = 1;
    g.nodes.push_back(n0);

    sema::NodeInstance n1;
    n1.type_id = 1;
    n1.phase = 0;
    n1.args_offset = 1;
    n1.args_count = 0;
    g.nodes.push_back(n1);

    sema::Literal lit;
    lit.kind = sema::Literal::Kind::Float32;
    lit.f = 1.5;
    g.args.push_back(sema::Arg::makeLiteral(lit));

    const std::vector<uint8_t> bytes = serialize(g);
    DeserializeResult dr = deserialize(ByteSpan(bytes), testRegistry());

    REQUIRE(dr.ok);
    REQUIRE(dr.graph != nullptr);
    REQUIRE(dr.graph->nodes.size() == 2);
    REQUIRE(dr.graph->args.size() == 1);

    CHECK(dr.graph->nodes[0].type_id == 0);
    CHECK(dr.graph->nodes[0].args_offset == 0);
    CHECK(dr.graph->nodes[0].args_count == 1);
    CHECK(dr.graph->nodes[1].type_id == 1);
    CHECK(dr.graph->nodes[1].args_count == 0);

    CHECK(sameArgs(dr.graph->args[0], g.args[0]));
}

TEST_CASE("roundtrip: resources and fields with a string default",
          "[serialization][roundtrip]")
{
    sema::Graph g;
    g.registry_fingerprint = sema::computeRegistryFingerprint(testRegistry());

    // The string pool holds "player" at offset 0.
    g.string_pool = {'p', 'l', 'a', 'y', 'e', 'r'};
    g.literal_pool_size = 6;

    sema::Resource r;
    r.name = "PlayerState";
    r.fields_offset = 0;
    r.fields_count = 2;
    g.resources.push_back(r);

    sema::ResourceField f0;
    f0.name = "hp";
    f0.type = sema::TypeId::primitive("int32");
    f0.defaultValue.kind = sema::Literal::Kind::Int32;
    f0.defaultValue.i = 100;
    f0.hasDefault = true;
    g.resource_fields.push_back(f0);

    sema::ResourceField f1;
    f1.name = "name";
    f1.type = sema::TypeId::primitive("string");
    f1.defaultValue = sema::Literal::makeString(0, 6);
    f1.hasDefault = true;
    g.resource_fields.push_back(f1);

    const std::vector<uint8_t> bytes = serialize(g);
    DeserializeResult dr = deserialize(ByteSpan(bytes), testRegistry());

    REQUIRE(dr.ok);
    REQUIRE(dr.graph != nullptr);

    REQUIRE(dr.graph->resources.size() == 1);
    CHECK(dr.graph->resources[0].name == "PlayerState");
    CHECK(dr.graph->resources[0].fields_offset == 0);
    CHECK(dr.graph->resources[0].fields_count == 2);

    REQUIRE(dr.graph->resource_fields.size() == 2);
    CHECK(dr.graph->resource_fields[0].name == "hp");
    CHECK(sameTypeId(dr.graph->resource_fields[0].type,
                     g.resource_fields[0].type));
    CHECK(dr.graph->resource_fields[0].hasDefault);
    CHECK(dr.graph->resource_fields[0].defaultValue.kind ==
          sema::Literal::Kind::Int32);
    CHECK(dr.graph->resource_fields[0].defaultValue.i == 100);

    CHECK(dr.graph->resource_fields[1].name == "name");
    CHECK(sameTypeId(dr.graph->resource_fields[1].type,
                     g.resource_fields[1].type));
    CHECK(dr.graph->resource_fields[1].hasDefault);
    CHECK(dr.graph->resource_fields[1].defaultValue.kind ==
          sema::Literal::Kind::String);
    CHECK(dr.graph->resource_fields[1].defaultValue.string.offset == 0);
    CHECK(dr.graph->resource_fields[1].defaultValue.string.length == 6);

    // The pool now holds the graph's literals and the interned names.
    // "player" must still be at offset 0, because the file's pool
    // starts with the graph's bytes.
    REQUIRE(dr.graph->string_pool.size() >= 6);
    CHECK(std::string(dr.graph->string_pool.begin(),
                      dr.graph->string_pool.begin() + 6) == "player");
}

TEST_CASE("roundtrip: a value node's literal, an inline node's reference, "
          "and the two orders",
          "[serialization][roundtrip]")
{
    sema::Graph g;
    g.registry_fingerprint = sema::computeRegistryFingerprint(testRegistry());

    // Node 0: Float32Node(2.5). Node 1: Float32Node(node 0).
    sema::NodeInstance n0;
    n0.type_id = 0;
    n0.args_offset = 0;
    n0.args_count = 1;
    g.nodes.push_back(n0);

    sema::NodeInstance n1;
    n1.type_id = 0;
    n1.args_offset = 1;
    n1.args_count = 1;
    g.nodes.push_back(n1);

    sema::Literal l;
    l.kind = sema::Literal::Kind::Float32;
    l.f = 2.5f;
    g.args.push_back(sema::Arg::makeLiteral(l));
    g.args.push_back(sema::Arg::makeNodeRef(0));

    g.value_order = {0, 1};

    const std::vector<uint8_t> bytes = serialize(g);
    DeserializeResult dr = deserialize(ByteSpan(bytes), testRegistry());

    REQUIRE(dr.ok);
    REQUIRE(dr.graph != nullptr);

    REQUIRE(dr.graph->nodes.size() == 2);
    REQUIRE(dr.graph->args.size() == 2);
    REQUIRE(dr.graph->value_order.size() == 2);

    CHECK(dr.graph->args[0].kind == sema::Arg::Kind::Literal);
    CHECK(dr.graph->args[0].literal.f == 2.5f);
    CHECK(dr.graph->args[1].kind == sema::Arg::Kind::NodeRef);
    CHECK(dr.graph->args[1].node_ref == 0);
    CHECK(dr.graph->value_order[0] == 0);
    CHECK(dr.graph->value_order[1] == 1);
}

TEST_CASE("roundtrip: identity — deserialize(serialize(g)) re-serializes "
          "to the same bytes",
          "[serialization][roundtrip]")
{
    sema::Graph g;
    g.registry_fingerprint = sema::computeRegistryFingerprint(testRegistry());

    sema::Resource r;
    r.name = "PlayerConfig";
    g.resources.push_back(r);
    sema::ResourceField f;
    f.name = "speed";
    f.type = sema::TypeId::primitive("float32");
    f.defaultValue.kind = sema::Literal::Kind::Float32;
    f.defaultValue.f = 200.0;
    f.hasDefault = true;
    g.resource_fields.push_back(f);

    const std::vector<uint8_t> first = serialize(g);
    DeserializeResult dr = deserialize(ByteSpan(first), testRegistry());
    REQUIRE(dr.ok);

    const std::vector<uint8_t> second = serialize(*dr.graph);
    CHECK(first == second);
}