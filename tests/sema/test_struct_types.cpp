/// @file tests/sema/test_struct_types.cpp
///
/// @brief Tests for struct-typed nodes and arguments.
///
/// A struct is a compound value type declared by the engine. A node
/// can accept a struct argument or produce a struct result. Struct
/// values are produced by node calls (`Vector2Node(1.0, 2.0)`).

#include "sema/Sema.hpp"
#include "sema/Registry.hpp"
#include "sema/Graph.hpp"
#include "sema/NodeKind.hpp"
#include "sema/TypeId.hpp"
#include "core/memory/ArenaSpan.hpp"
#include "core/diagnostics/DiagCode.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lucid::sema;

namespace
{

    /// A registry with a Vector2 struct, a constructor value node
    /// (`Vector2Node`), and a consumer value node
    /// (`Vector2LengthSquaredNode`).
    const Registry &testRegistry()
    {
        static const PhaseInfo phases[] = {{"update"}};

        static const StructFieldInfo vec2Fields[] = {
            {"x", TypeId::primitive("float32")},
            {"y", TypeId::primitive("float32")},
        };
        static const StructTypeInfo structs[] = {
            {"Vector2", ArenaSpan<StructFieldInfo>(vec2Fields, 2)},
        };

        static const NodeArgInfo vec2CtorArgs[] = {
            {"x", TypeId::primitive("float32")},
            {"y", TypeId::primitive("float32")},
        };
        static const NodeArgInfo vec2ConsumerArgs[] = {
            {"v", TypeId::structType("Vector2")},
        };

        static const NodeTypeInfo nodeTypes[] = {
            // 0: Vector2Node(x, y) -> Vector2
            {"Vector2Node", NodeKind::Value, "Math", 0,
             ArenaSpan<NodeArgInfo>(vec2CtorArgs, 2),
             TypeId::structType("Vector2")},
            // 1: Vector2LengthSquaredNode(v) -> float32
            {"Vector2LengthSquaredNode", NodeKind::Value, "Math", 0,
             ArenaSpan<NodeArgInfo>(vec2ConsumerArgs, 1),
             TypeId::primitive("float32")},
            // 2: PrintFloatNode(v) -> no result, an action.
            //    The action takes a float32 and prints it.
            {"PrintFloatNode", NodeKind::Action, "Debug", 0,
             ArenaSpan<NodeArgInfo>(vec2ConsumerArgs + 0, 0), // no args
             TypeId{}},
        };

        static const Registry reg = []
        {
            Registry r;
            r.phases = ArenaSpan<PhaseInfo>(phases, 1);
            r.structs = ArenaSpan<StructTypeInfo>(structs, 1);
            r.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes, 3);
            return r;
        }();

        return reg;
    }

    std::unique_ptr<Graph> compileOrFail(std::string_view source)
    {
        auto result = compile(source, "test.lucid", testRegistry());
        if (!result.ok)
        {
            std::string joined;
            for (const auto &d : result.diagnostics)
            {
                joined += "  ";
                joined += d.message;
                joined += "\n";
            }
            FAIL("compile failed:\n"
                 << joined);
        }
        return std::move(result.graph);
    }

    bool hasCode(const std::vector<lucid::diag::Diagnostic> &diags,
                 lucid::diag::DiagCode code)
    {
        for (const auto &d : diags)
            if (d.code == code)
                return true;
        return false;
    }

} // namespace

TEST_CASE("sema: a struct-producing node compiles",
          "[sema][struct]")
{
    constexpr std::string_view source = R"(
        node v = Vector2Node(1.0, 2.0)
    )";

    auto graph = compileOrFail(source);
    REQUIRE(graph != nullptr);
    REQUIRE(graph->nodes.size() == 1);
    CHECK(graph->nodes[0].type_id == 0);
}

TEST_CASE("sema: a struct-consuming node accepts a struct",
          "[sema][struct]")
{
    constexpr std::string_view source = R"(
        node v  = Vector2Node(1.0, 2.0)
        node ls = Vector2LengthSquaredNode(v)
    )";

    auto graph = compileOrFail(source);
    REQUIRE(graph != nullptr);
    REQUIRE(graph->nodes.size() == 2);
    // Node 1's argument is a NodeRef to node 0.
    const auto args = graph->argsOf(graph->nodes[1]);
    REQUIRE(args.size() == 1);
    CHECK(args[0].kind == Arg::Kind::NodeRef);
    CHECK(args[0].node_ref == 0);
}

TEST_CASE("sema: a scalar argument in a struct slot is rejected",
          "[sema][struct]")
{
    constexpr std::string_view source = R"(
        node ls = Vector2LengthSquaredNode(1.0)
    )";

    auto result = compile(source, "test.lucid", testRegistry());
    REQUIRE_FALSE(result.ok);
    CHECK(hasCode(result.diagnostics, lucid::diag::DiagCode::Type_Mismatch));
}

TEST_CASE("sema: a struct argument in a scalar slot is rejected",
          "[sema][struct]")
{
    // Vector2Node(x, y) takes two float32. Passing a Vector2 for `x`
    // is a mismatch.
    constexpr std::string_view source = R"(
        node v  = Vector2Node(1.0, 2.0)
        node v2 = Vector2Node(v, 3.0)
    )";

    auto result = compile(source, "test.lucid", testRegistry());
    REQUIRE_FALSE(result.ok);
    CHECK(hasCode(result.diagnostics, lucid::diag::DiagCode::Type_Mismatch));
}

TEST_CASE("sema: a struct-typed resource field is zero-initialized",
          "[sema][struct]")
{
    constexpr std::string_view source = R"(
        resource R {
            origin: Vector2,
        }
    )";

    auto graph = compileOrFail(source);
    REQUIRE(graph != nullptr);
    REQUIRE(graph->resource_fields.size() == 1);

    const auto &f = graph->resource_fields[0];
    CHECK(f.name == "origin");
    CHECK(f.type.kind == TypeId::Kind::Struct);
    CHECK(f.type.name == "Vector2");

    // The field's value is a struct literal.
    CHECK(f.value.kind == Literal::Kind::Struct);
    // Its bytes are in the struct-value pool: two float32 zeros = 8 bytes.
    REQUIRE(graph->struct_value_pool.size() >= f.value.struct_value.length);
    CHECK(f.value.struct_value.length == 8); // 4 + 4
}

TEST_CASE("sema: a struct-typed resource field cannot have a default",
          "[sema][struct]")
{
    constexpr std::string_view source = R"(
        resource R {
            origin: Vector2 = Vector2Node(1.0, 2.0),
        }
    )";

    auto result = compile(source, "test.lucid", testRegistry());
    REQUIRE_FALSE(result.ok);
    CHECK(hasCode(result.diagnostics,
                  lucid::diag::DiagCode::Type_InvalidDefault));
}