/// @file tests/sema/test_graph.cpp
///
/// @brief Tests for the Graph data types.
#include "sema/Graph.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lucid::sema;

TEST_CASE("Graph is empty by default", "[sema][graph]")
{
    Graph g;
    CHECK(g.nodes.empty());
    CHECK(g.args.empty());
    CHECK(g.resources.empty());
    CHECK(g.resource_fields.empty());
    CHECK(g.subscribers.empty());
    CHECK(g.phase_order.empty());
    CHECK(g.value_order.empty());
    CHECK(g.string_pool.empty());
    CHECK(g.registry_fingerprint == 0);
}

TEST_CASE("Arg literals", "[sema][graph]")
{
    Literal lit{'\0'};   // use char constructor to get a non-nil Literal
    lit.kind = Literal::Kind::Int32;
    lit.i    = 42;

    Arg a = Arg::makeLiteral(lit);
    CHECK(a.kind          == Arg::Kind::Literal);
    CHECK(a.literal.kind  == Literal::Kind::Int32);
    CHECK(a.literal.i     == 42);
}

TEST_CASE("Arg node references", "[sema][graph]")
{
    Arg a = Arg::makeNodeRef(7);
    CHECK(a.kind     == Arg::Kind::NodeRef);
    CHECK(a.node_ref == 7);
}

TEST_CASE("Arg resource references", "[sema][graph]")
{
    Arg a = Arg::makeResourceRef(3, 2);
    CHECK(a.kind                        == Arg::Kind::ResourceRef);
    CHECK(a.resource_ref.resource_index == 3);
    CHECK(a.resource_ref.field_index    == 2);
}

TEST_CASE("Graph argsOf slices correctly", "[sema][graph]")
{
    Graph g;
    g.args.push_back(Arg::makeNodeRef(0));
    g.args.push_back(Arg::makeNodeRef(1));
    g.args.push_back(Arg::makeNodeRef(2));

    NodeInstance n;
    n.args_offset = 1;
    n.args_count  = 2;

    auto slice = g.argsOf(n);
    REQUIRE(slice.size() == 2);
    CHECK(slice[0].node_ref == 1);
    CHECK(slice[1].node_ref == 2);
}

TEST_CASE("Graph fieldsOf slices correctly", "[sema][graph]")
{
    Graph g;

    ResourceField f0;
    f0.name = "a";
    f0.type = TypeId::primitive("int32");

    ResourceField f1;
    f1.name = "b";
    f1.type = TypeId::primitive("float32");

    g.resource_fields.push_back(f0);
    g.resource_fields.push_back(f1);

    Resource r;
    r.fields_offset = 0;
    r.fields_count  = 2;

    auto slice = g.fieldsOf(r);
    REQUIRE(slice.size() == 2);
    CHECK(slice[0].name == "a");
    CHECK(slice[1].name == "b");
}

TEST_CASE("Graph subscribersOf slices correctly", "[sema][graph]")
{
    Graph g;
    g.subscribers.push_back(10);
    g.subscribers.push_back(11);
    g.subscribers.push_back(12);

    NodeInstance n;
    n.subscribers_offset = 1;
    n.subscribers_count  = 2;

    auto slice = g.subscribersOf(n);
    REQUIRE(slice.size() == 2);
    CHECK(slice[0] == 11);
    CHECK(slice[1] == 12);
}
