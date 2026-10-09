/// @file sema/SymbolTable.hpp
///
/// @brief A per-module symbol table.
///
/// ─── What a symbol table is ───────────────────────────────────────────────
/// The set of names a module declares, mapped to the declarations that
/// introduce them.
///
/// ─── One table per module ─────────────────────────────────────────────────
/// Sema builds one SymbolTable per module. Cross-module name resolution
/// uses the import table (Step 7.8).
///
/// ─── What the table stores ────────────────────────────────────────────────
/// A symbol is a name, a kind, and a pointer to the declaration.
///
/// ─── Duplicate names ──────────────────────────────────────────────────────
/// Adding a symbol with a name that already exists reports
/// Name_Redeclaration and returns false.

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
        Import,   // an imported module's name binding
        Enum,     // an enum type
        Resource, // a resource
        Node,     // a node
    };

    /// @brief The name of a symbol kind, for diagnostics.
    inline const char *symbolKindName(SymbolKind k) noexcept
    {
        switch (k)
        {
        case SymbolKind::Import:
            return "import";
        case SymbolKind::Enum:
            return "enum";
        case SymbolKind::Resource:
            return "resource";
        case SymbolKind::Node:
            return "node";
        }
        return "symbol";
    }

    /// @brief One entry in a symbol table.
    struct Symbol
    {
        InternedString name;
        SymbolKind kind;
        DeclAST *decl;
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
        bool add(InternedString name,
                 SymbolKind kind,
                 DeclAST *decl,
                 lucid::diag::DiagnosticEngine &diag);

        // ─── Query ─────────────────────────────────────────────────────────

        const Symbol *find(InternedString name) const noexcept;

        size_t size() const noexcept { return m_symbols.size(); }
        bool empty() const noexcept { return m_symbols.empty(); }
        const std::vector<Symbol> &all() const noexcept { return m_symbols; }

    private:
        std::vector<Symbol> m_symbols;
    };

} // namespace lucid::sema