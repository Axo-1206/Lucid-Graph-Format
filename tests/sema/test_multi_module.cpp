/// @file tests/sema/test_multi_module.cpp
///
/// @brief End-to-end tests for multi-module compiles.

#include "sema/Sema.hpp"
#include "sema/Graph.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace lucid::sema;

namespace
{

    // A TestRegistry identical to the one in test_type_checker.cpp.
    // (Paste a compatible registry here.)

    struct TestRegistry
    {
        std::vector<PhaseInfo> phases;
        std::vector<HandleTypeInfo> handles;
        std::vector<NodeArgInfo> floatArgs;
        std::vector<NodeTypeInfo> nodeTypes;
        Registry registry;

        TestRegistry()
        {
            phases.push_back(PhaseInfo{"update"});
            handles.push_back(HandleTypeInfo{"BodyRef"});

            floatArgs.push_back(NodeArgInfo{"value",
                                            TypeId::primitive("float32")});
            nodeTypes.push_back(NodeTypeInfo{
                "Float32Node", NodeKind::Value, "Math", 0,
                ArenaSpan<NodeArgInfo>(floatArgs.data(), floatArgs.size()),
                TypeId::primitive("float32")});

            registry.phases = ArenaSpan<PhaseInfo>(phases.data(),
                                                   phases.size());
            registry.handles = ArenaSpan<HandleTypeInfo>(handles.data(),
                                                         handles.size());
            registry.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes.data(),
                                                         nodeTypes.size());
        }
    };

    /// Run `compile` on a root source with the given imports.
    CompileResult compileWithModules(
        std::string_view rootSource,
        std::string_view rootFilename,
        std::unordered_map<std::string, std::string> modules,
        const Registry &registry)
    {
        CompileOptions options;
        options.loadModule = [modules = std::move(modules)](
                                 std::string_view path)
            -> std::optional<std::string>
        {
            auto it = modules.find(std::string(path));
            if (it == modules.end())
                return std::nullopt;
            return it->second;
        };

        return compile(rootSource, rootFilename, registry, options);
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// A single module still works
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("compile handles a single module with no imports",
          "[sema][multi-module]")
{
    TestRegistry reg;
    auto result = compileWithModules(
        "node x = Float32Node(1.5)\n",
        "main.lucid",
        {},
        reg.registry);

    REQUIRE(result.ok);
    REQUIRE(result.graph != nullptr);
    CHECK(result.graph->nodes.size() == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Two modules, one import
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("compile handles an imported exported enum",
          "[sema][multi-module]")
{
    TestRegistry reg;
    auto result = compileWithModules(
        "import keys\n"
        "resource R { k: Key = Key.W }\n",
        "main.lucid",
        {{"keys", "@export enum Key { W, A }\n"}},
        reg.registry);

    REQUIRE(result.ok);
    REQUIRE(result.graph != nullptr);
    CHECK(result.graph->resources.size() == 1);
}

TEST_CASE("compile rejects a non-exported declaration",
          "[sema][multi-module]")
{
    TestRegistry reg;
    // `Key` is declared in `keys` but not exported, so `main` cannot
    // see it.
    auto result = compileWithModules(
        "import keys\n"
        "resource R { k: Key = Key.W }\n",
        "main.lucid",
        {{"keys", "enum Key { W, A }\n"}},
        reg.registry);

    // The reference to `Key` is unresolved.
    CHECK_FALSE(result.ok);
    CHECK(result.graph == nullptr);
}

TEST_CASE("compile handles a qualified type reference",
          "[sema][multi-module]")
{
    TestRegistry reg;
    auto result = compileWithModules(
        "import keys\n"
        "resource R { k: keys::Key = Key.W }\n",
        "main.lucid",
        {{"keys", "@export enum Key { W, A }\n"}},
        reg.registry);

    REQUIRE(result.ok);
    REQUIRE(result.graph != nullptr);
}

TEST_CASE("compile merges nodes from multiple modules",
          "[sema][multi-module]")
{
    TestRegistry reg;
    auto result = compileWithModules(
        "import helpers\n"
        "node a = Float32Node(1.0)\n"
        "node b = Float32Node(a)\n",
        "main.lucid",
        {{"helpers",
          "@export node h = Float32Node(2.0)\n"}},
        reg.registry);

    REQUIRE(result.ok);
    REQUIRE(result.graph != nullptr);
    // Two nodes from main, one from helpers.
    CHECK(result.graph->nodes.size() >= 3);
}

// ─────────────────────────────────────────────────────────────────────────────
// Errors
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("compile reports a missing import",
          "[sema][multi-module]")
{
    TestRegistry reg;
    auto result = compileWithModules(
        "import missing\n",
        "main.lucid",
        {},
        reg.registry);

    CHECK_FALSE(result.ok);
    CHECK_FALSE(result.diagnostics.empty());
}

TEST_CASE("compile reports an @export on an import",
          "[sema][multi-module]")
{
    TestRegistry reg;
    auto result = compileWithModules(
        "@export import keys\n",
        "main.lucid",
        {{"keys", ""}},
        reg.registry);

    CHECK_FALSE(result.ok);
    bool found = false;
    for (const auto &d : result.diagnostics)
    {
        if (d.code == lucid::diag::DiagCode::Attr_ExportOnImport)
        {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("compile accepts @export on an enum",
          "[sema][multi-module]")
{
    TestRegistry reg;
    auto result = compileWithModules(
        "@export enum Key { W, A }\n",
        "main.lucid",
        {},
        reg.registry);

    CHECK(result.ok);
}