/// @file sema/CompositeScope.hpp
///
/// @brief A composite's local scope.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A composite's body can reference four kinds of names: the
/// composite's inputs, its internal declarations, the enclosing
/// module's symbols, and (inside the composite) imported aliases.
///
/// A CompositeScope holds the local table (inputs and internal
/// declarations). It does NOT hold a reference to the enclosing
/// module's table. Lookup takes the module's table as a parameter,
/// which avoids a dangling-reference hazard when the scope is
/// moved or returned by value.
///
/// ─── Why not hold the module table by reference ───────────────────────────
/// An earlier design stored a `const SymbolTable&` to the module
/// table. That design is fragile: if a CompositeScope is a member of
/// a larger struct that is moved (returned by value, stored in a
/// container, etc.), the reference points at the old location of the
/// module table, which may be destroyed. Passing the module table to
/// `find` sidesteps this entirely.

#pragma once

#include "core/ast/DeclAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/InternedString.hpp"
#include "sema/SymbolTable.hpp"

namespace lucid::sema
{

    class CompositeScope
    {
    public:
        CompositeScope() = default;

        // ─── Building ──────────────────────────────────────────────────────

        /// @brief Add a composite input to the local table.
        bool addInput(CompositeInputAST *input,
                      lucid::diag::DiagnosticEngine &diag);

        /// @brief Add a local declaration to the local table.
        bool addLocal(InternedString name,
                      SymbolKind kind,
                      BaseAST *decl,
                      lucid::diag::DiagnosticEngine &diag);

        // ─── Query ─────────────────────────────────────────────────────────

        /// @brief Find a name in the local scope only.
        const Symbol *findLocal(InternedString name) const noexcept
        {
            return m_local.find(name);
        }

        /// @brief Find a name in the local scope, falling back to the
        ///        given module symbol table.
        ///
        /// The caller supplies the module table. It must outlive the
        /// returned pointer (which is a pointer into either the local
        /// or the module table).
        const Symbol *find(InternedString name,
                           const SymbolTable &moduleSymbols) const noexcept;

        // ─── Accessors ─────────────────────────────────────────────────────

        const SymbolTable &localSymbols() const noexcept { return m_local; }

    private:
        SymbolTable m_local;
    };

    // ─── Helper ───────────────────────────────────────────────────────────────

    /// @brief Build a composite's local scope from its declaration.
    ///
    /// Adds the composite's inputs and its body declarations to a fresh
    /// CompositeScope. Does NOT reference the module's table; the
    /// caller supplies the module table to `find`.
    CompositeScope buildCompositeScope(const CompositeDeclAST *decl,
                                       lucid::diag::DiagnosticEngine &diag);

} // namespace lucid::sema