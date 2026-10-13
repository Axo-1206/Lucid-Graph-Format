/// @file tests/sema/test_registry_validation.cpp
///
/// @brief Tests for `validateRegistry`.
///
/// The registry is validated at the start of every compile. A
/// malformed registry reports a diagnostic and refuses the compile.

#include "sema/Registry.hpp"
#include "sema/NodeKind.hpp"
#include "sema/TypeId.hpp"
#include "sema/Sema.hpp"
#include "core/memory/ArenaSpan.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace lucid::sema;

namespace
{

    /// Compile a tiny source against `registry` and return the
    /// diagnostics. A malformed registry fails before parsing.
    std::vector<lucid::diag::Diagnostic>
    compileAgainst(const Registry &registry)
    {
        auto result = compile("node x = NothingNode()\n",
                              "test.lucid", registry);
        return result.diagnostics;
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

TEST_CASE("validateRegistry: a well-formed registry passes",
          "[sema][registry]")
{
    // Empty registry: no enums, no structs, no node types. Trivially
    // well-formed.
    Registry r;
    lucid::diag::DiagnosticEngine diag;
    CHECK(validateRegistry(r, diag));
    CHECK(diag.all().empty());
}

TEST_CASE("validateRegistry: an enum with no zero member is rejected",
          "[sema][registry]")
{
    static const EnumMemberInfo members[] = {
        {"A", 1},
        {"B", 2},
    };
    static const EnumTypeInfo enums[] = {
        {"Direction", ArenaSpan<EnumMemberInfo>(members, 2)},
    };

    Registry r;
    r.enums = ArenaSpan<EnumTypeInfo>(enums, 1);

    lucid::diag::DiagnosticEngine diag;
    CHECK_FALSE(validateRegistry(r, diag));
    CHECK(hasCode(diag.all(), lucid::diag::DiagCode::Registry_EnumWithoutZero));
}

TEST_CASE("validateRegistry: a struct with an unknown field type is rejected",
          "[sema][registry]")
{
    static const StructFieldInfo fields[] = {
        {"x", TypeId::structType("Nonexistent")},
    };
    static const StructTypeInfo structs[] = {
        {"Wrapper", ArenaSpan<StructFieldInfo>(fields, 1)},
    };

    Registry r;
    r.structs = ArenaSpan<StructTypeInfo>(structs, 1);

    lucid::diag::DiagnosticEngine diag;
    CHECK_FALSE(validateRegistry(r, diag));
    CHECK(hasCode(diag.all(), lucid::diag::DiagCode::Registry_UnknownFieldType));
}

TEST_CASE("validateRegistry: a self-referential struct is rejected",
          "[sema][registry]")
{
    // A struct whose field is a struct of its own type.
    static const StructFieldInfo fieldsA[] = {
        {"self", TypeId::structType("A")},
    };
    static const StructTypeInfo structs[] = {
        {"A", ArenaSpan<StructFieldInfo>(fieldsA, 1)},
    };

    Registry r;
    r.structs = ArenaSpan<StructTypeInfo>(structs, 1);

    lucid::diag::DiagnosticEngine diag;
    CHECK_FALSE(validateRegistry(r, diag));
    CHECK(hasCode(diag.all(), lucid::diag::DiagCode::Registry_StructCycle));
}

TEST_CASE("validateRegistry: two mutually referential structs are rejected",
          "[sema][registry]")
{
    static const StructFieldInfo fieldsA[] = {
        {"b", TypeId::structType("B")},
    };
    static const StructFieldInfo fieldsB[] = {
        {"a", TypeId::structType("A")},
    };
    static const StructTypeInfo structs[] = {
        {"A", ArenaSpan<StructFieldInfo>(fieldsA, 1)},
        {"B", ArenaSpan<StructFieldInfo>(fieldsB, 1)},
    };

    Registry r;
    r.structs = ArenaSpan<StructTypeInfo>(structs, 2);

    lucid::diag::DiagnosticEngine diag;
    CHECK_FALSE(validateRegistry(r, diag));
    CHECK(hasCode(diag.all(), lucid::diag::DiagCode::Registry_StructCycle));
}

TEST_CASE("validateRegistry: a struct referring to a struct is fine",
          "[sema][registry]")
{
    static const StructFieldInfo innerFields[] = {
        {"x", TypeId::primitive("float32")},
        {"y", TypeId::primitive("float32")},
    };
    static const StructFieldInfo outerFields[] = {
        {"position", TypeId::structType("Vector2")},
        {"scale", TypeId::primitive("float32")},
    };
    static const StructTypeInfo structs[] = {
        {"Vector2", ArenaSpan<StructFieldInfo>(innerFields, 2)},
        {"Transform", ArenaSpan<StructFieldInfo>(outerFields, 2)},
    };

    Registry r;
    r.structs = ArenaSpan<StructTypeInfo>(structs, 2);

    lucid::diag::DiagnosticEngine diag;
    CHECK(validateRegistry(r, diag));
}