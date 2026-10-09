#pragma once

#include "sema/Sema.hpp"
#include "sema/dump/GraphDumper.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lucid::tests::sema_fixtures
{

    namespace fs = std::filesystem;

    struct FixtureData
    {
        std::string input;
        std::unordered_map<std::string, std::string> modules;
    };

    inline std::string readFile(const fs::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    inline FixtureData loadFixture(const fs::path &dir)
    {
        FixtureData data;
        data.input = readFile(dir / "input.lucid");

        for (const auto &entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file() ||
                entry.path().extension() != ".lucid")
                continue;

            const std::string stem = entry.path().stem().string();
            if (stem != "input")
                data.modules[stem] = readFile(entry.path());
        }

        return data;
    }

    struct FixtureRegistry
    {
        std::vector<sema::PhaseInfo> phases;
        std::vector<sema::EnumMemberInfo> keyMembers;
        std::vector<sema::EnumTypeInfo> enums;
        std::vector<sema::HandleTypeInfo> handles;
        std::vector<sema::NodeArgInfo> floatArgs;
        std::vector<sema::NodeArgInfo> actionArgs;
        std::vector<sema::NodeArgInfo> printArgs;
        std::vector<sema::NodeTypeInfo> nodeTypes;
        sema::Registry registry;

        FixtureRegistry()
        {
            phases.push_back({"update"});
            keyMembers = {{"W", 0}, {"A", 1}, {"S", 2}, {"D", 3}};
            enums.push_back(
                {"Key", ArenaSpan<sema::EnumMemberInfo>(
                            keyMembers.data(), keyMembers.size())});
            handles.push_back({"BodyRef"});

            floatArgs.push_back({"value", sema::TypeId::primitive("float32")});
            nodeTypes.push_back(
                {"Float32Node", sema::NodeKind::Value, "Math", 0,
                 ArenaSpan<sema::NodeArgInfo>(floatArgs.data(),
                                              floatArgs.size()),
                 sema::TypeId::primitive("float32")});

            actionArgs.push_back({"body", sema::TypeId::handle("BodyRef")});
            nodeTypes.push_back(
                {"MoveBody", sema::NodeKind::Action, "Physics", 0,
                 ArenaSpan<sema::NodeArgInfo>(actionArgs.data(),
                                              actionArgs.size()),
                 sema::TypeId{}});

            printArgs.push_back({"value", sema::TypeId::primitive("float32")});
            nodeTypes.push_back(
                {"PrintNode", sema::NodeKind::Action, "Debug", 0,
                 ArenaSpan<sema::NodeArgInfo>(printArgs.data(),
                                              printArgs.size()),
                 sema::TypeId{}});

            nodeTypes.push_back(
                {"EveryFrame", sema::NodeKind::Trigger, "Flow", 0,
                 ArenaSpan<sema::NodeArgInfo>{}, sema::TypeId{}});

            registry.phases =
                ArenaSpan<sema::PhaseInfo>(phases.data(), phases.size());
            registry.enums =
                ArenaSpan<sema::EnumTypeInfo>(enums.data(), enums.size());
            registry.handles =
                ArenaSpan<sema::HandleTypeInfo>(handles.data(), handles.size());
            registry.nodeTypes =
                ArenaSpan<sema::NodeTypeInfo>(nodeTypes.data(),
                                              nodeTypes.size());
        }
    };

    inline std::string compileAndDump(const FixtureData &data,
                                      const sema::Registry &registry)
    {
        sema::CompileOptions options;
        options.loadModule = [&data](std::string_view path)
            -> std::optional<std::string>
        {
            const auto it = data.modules.find(std::string(path));
            if (it == data.modules.end())
                return std::nullopt;
            return it->second;
        };

        const sema::CompileResult result =
            sema::compile(data.input, "input.lucid", registry, options);
        if (!result.ok || !result.graph)
            return {};

        return sema::dump::dumpGraph(*result.graph);
    }

} // namespace lucid::tests::sema_fixtures
