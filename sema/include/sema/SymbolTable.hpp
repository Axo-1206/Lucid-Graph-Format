/// @file sema/SymbolTable.hpp
///
/// @brief A per-module or per-composite symbol table.
///
/// ─── What a symbol table is ───────────────────────────────────────────────
/// The set of names a module (or a composite) declares, mapped to the
/// declarations that introduce them.
///
/// ─── One table per scope ──────────────────────────────────────────────────
/// A module has a table. A composite has its own local table for
/// inputs and internal declarations. A composite's full scope is the
/// local table plus the enclosing module's table (see CompositeScope).
///
/// ─── What the table stores ────────────────────────────────────────────────
/// A symbol is a name, a kind, and a pointer to the declaration. The
/// pointer is a BaseAST*, not a DeclAST*, because composite inputs are
/// not DeclASTs (they derive from BaseAST directly).
///
/// ─── Duplicate names ──────────────────────────────────────────────────────
/// Adding a symbol with a name that already exists reports
/// Name_Redeclaration and returns false.

#pragma once

#include "core/ast/BaseAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/InternedString.hpp"

#include <cstdint>
#include <vector>

namespace lucid::sema
{

    /// @brief The kind of a symbol.
    enum class SymbolKind : uint8_t
    {
        Import,         // an import alias
        Enum,           // an enum type
        Resource,       // a resource
        Node,           // a node
        Composite,      // a composite
        CompositeInput, // a composite input (local scope only)
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
        case SymbolKind::Composite:
            return "composite";
        case SymbolKind::CompositeInput:
            return "composite input";
        }
        return "symbol";
    }

    /// @brief One entry in a symbol table.
    struct Symbol
    {
        InternedString name;
        SymbolKind kind;
        BaseAST *decl; // DeclAST* or CompositeInputAST*
    };

    /// @brief A symbol table.
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
                 BaseAST *decl,
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
