/// @file tests/sema/test_symbol_table.cpp
///
/// @brief Tests for SymbolTable.

#include "sema/SymbolTable.hpp"

#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/InternedString.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

using lucid::diag::DiagnosticEngine;
using lucid::sema::SymbolKind;
using lucid::sema::SymbolTable;
using lucid::sema::symbolKindName;

namespace
{

    struct Fixture
    {
        StringPool pool;
        DiagnosticEngine diag{&pool};

        InternedString name(std::string_view s)
        {
            return pool.intern(s);
        }
    };

} // namespace

TEST_CASE("SymbolTable is empty by default", "[sema][symbol-table]")
{
    SymbolTable table;
    CHECK(table.empty());
    CHECK(table.size() == 0);
}

TEST_CASE("SymbolTable::add stores a symbol", "[sema][symbol-table]")
{
    Fixture f;
    SymbolTable table;

    const bool added = table.add(f.name("Foo"),
                                 SymbolKind::Node,
                                 nullptr,
                                 f.diag);
    CHECK(added);
    CHECK(table.size() == 1);
    CHECK_FALSE(table.empty());
    CHECK_FALSE(f.diag.hasErrors());
}

TEST_CASE("SymbolTable::find returns a stored symbol", "[sema][symbol-table]")
{
    Fixture f;
    SymbolTable table;

    table.add(f.name("Foo"), SymbolKind::Node, nullptr, f.diag);
    table.add(f.name("Bar"), SymbolKind::Resource, nullptr, f.diag);

    const auto* s = table.find(f.name("Bar"));
    REQUIRE(s != nullptr);
    CHECK(s->kind == SymbolKind::Resource);
}

TEST_CASE("SymbolTable::find returns nullptr for an unknown name",
          "[sema][symbol-table]")
{
    Fixture f;
    SymbolTable table;
    table.add(f.name("Foo"), SymbolKind::Node, nullptr, f.diag);

    CHECK(table.find(f.name("Missing")) == nullptr);
}

TEST_CASE("SymbolTable reports a duplicate name", "[sema][symbol-table]")
{
    Fixture f;
    SymbolTable table;

    const bool first = table.add(f.name("Foo"),
                                 SymbolKind::Node,
                                 nullptr,
                                 f.diag);
    const bool second = table.add(f.name("Foo"),
                                  SymbolKind::Resource,
                                  nullptr,
                                  f.diag);

    CHECK(first);
    CHECK_FALSE(second);
    CHECK(table.size() == 1);
    CHECK(f.diag.hasErrors());
    CHECK(f.diag.errorCount() == 1);
    CHECK(f.diag.all()[0].code == lucid::diag::DiagCode::Name_Redeclaration);
}

TEST_CASE("SymbolTable keeps the first declaration on collision",
          "[sema][symbol-table]")
{
    Fixture f;
    SymbolTable table;

    table.add(f.name("Foo"), SymbolKind::Node, nullptr, f.diag);
    table.add(f.name("Foo"), SymbolKind::Resource, nullptr, f.diag);

    const auto* s = table.find(f.name("Foo"));
    REQUIRE(s != nullptr);
    CHECK(s->kind == SymbolKind::Node);
}

TEST_CASE("SymbolTable records multiple duplicates",
          "[sema][symbol-table]")
{
    Fixture f;
    SymbolTable table;

    table.add(f.name("Foo"), SymbolKind::Node, nullptr, f.diag);
    table.add(f.name("Foo"), SymbolKind::Node, nullptr, f.diag);
    table.add(f.name("Foo"), SymbolKind::Node, nullptr, f.diag);

    CHECK(table.size() == 1);
    CHECK(f.diag.errorCount() == 2);
}

TEST_CASE("symbolKindName returns stable names", "[sema][symbol-table]")
{
    CHECK(std::string_view(symbolKindName(SymbolKind::Import))    == "import");
    CHECK(std::string_view(symbolKindName(SymbolKind::Enum))      == "enum");
    CHECK(std::string_view(symbolKindName(SymbolKind::Resource))  == "resource");
    CHECK(std::string_view(symbolKindName(SymbolKind::Node))      == "node");
    CHECK(std::string_view(symbolKindName(SymbolKind::Composite)) == "composite");
}
