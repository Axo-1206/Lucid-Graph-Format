/// @file sema/SymbolTable.hpp
///
/// @brief A per-module symbol table.
///
/// ─── What a symbol table is ───────────────────────────────────────────────
/// The set of names a module declares, mapped to the declarations that
/// introduce them. Every top-level declaration with a name introduces
/// a symbol: imports (which introduce an alias), enums, resources,
/// nodes, and composites.
///
/// ─── One table per module ─────────────────────────────────────────────────
/// Sema builds one SymbolTable per module. Cross-module name resolution
/// (looking up a name through an import alias) uses a module's table as
/// a lookup structure, but a symbol table does not span modules.
///
/// ─── What the table stores ────────────────────────────────────────────────
/// A symbol is a name, a kind, and a pointer to the declaration. It
/// does not store resolution annotations, type information, or anything
/// computed by later passes. Those live in separate per-pass data
/// structures. The symbol table is Pass 1's only output.
///
/// ─── Duplicate names ──────────────────────────────────────────────────────
/// Adding a symbol with a name that already exists reports
/// Name_Redeclaration and returns false. The first declaration wins;
/// the colliding one is not stored. Every subsequent collision is
/// reported separately.

#pragma once

#include "core/ast/DeclAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/InternedString.hpp"

#include <cstdint>
#include <vector>

namespace lucid::sema
{

    /// @brief The kind of a symbol.
    enum class SymbolKind : uint8_t
    {
        Import,     // an import alias
        Enum,       // an enum type
        Resource,   // a resource
        Node,       // a node
        Composite,  // a composite
    };

    /// @brief The name of a symbol kind, for diagnostics.
    inline const char* symbolKindName(SymbolKind k) noexcept
    {
        switch (k)
        {
        case SymbolKind::Import:    return "import";
        case SymbolKind::Enum:      return "enum";
        case SymbolKind::Resource:  return "resource";
        case SymbolKind::Node:      return "node";
        case SymbolKind::Composite: return "composite";
        }
        return "symbol";
    }

    /// @brief One entry in a module's symbol table.
    struct Symbol
    {
        InternedString name;
        SymbolKind     kind;
        DeclAST*       decl;
    };

    /// @brief A module's symbol table.
    class SymbolTable
    {
    public:
        SymbolTable() = default;

        // ─── Building ──────────────────────────────────────────────────────

        /// @brief Add a symbol.
        ///
        /// If `name` is already in the table, reports a
        /// Name_Redeclaration diagnostic against `decl` and returns
        /// false. Otherwise adds the symbol and returns true.
        ///
        /// The first declaration with a given name wins. Subsequent
        /// declarations with the same name are rejected.
        bool add(InternedString name,
                 SymbolKind kind,
                 DeclAST* decl,
                 lucid::diag::DiagnosticEngine& diag);

        // ─── Query ─────────────────────────────────────────────────────────

        /// @brief Find a symbol by name. Returns nullptr if not found.
        const Symbol* find(InternedString name) const noexcept;

        /// @brief The number of symbols in the table.
        size_t size() const noexcept { return m_symbols.size(); }

        /// @brief True if the table has no symbols.
        bool empty() const noexcept { return m_symbols.empty(); }

        /// @brief All symbols, in insertion order.
        const std::vector<Symbol>& all() const noexcept { return m_symbols; }

    private:
        std::vector<Symbol> m_symbols;

        // Linear lookup. Symbol tables for typical modules have fewer
        // than a few hundred entries, so a linear scan is competitive
        // with a hash map, and it preserves insertion order for `all()`.
        //
        // If a module ever has thousands of symbols, this becomes a
        // hash map keyed on InternedString's id.
    };

} // namespace lucid::sema
