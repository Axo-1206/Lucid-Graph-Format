/// @file sema/src/sema/SymbolTable.cpp
///
/// @brief Implementation of SymbolTable.

#include "sema/SymbolTable.hpp"

#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/StringPool.hpp"

namespace lucid::sema
{

    bool SymbolTable::add(InternedString name,
                          SymbolKind kind,
                          DeclAST *decl,
                          lucid::diag::DiagnosticEngine &diag)
    {
        // ─── Duplicate check ───────────────────────────────────────────────
        const Symbol *existing = find(name);
        if (existing != nullptr)
        {
            // Report the collision. The message names both the
            // colliding declaration and the kind of the existing one.
            diag.error(lucid::diag::DiagCode::Name_Redeclaration, decl,
                       "redeclaration of '",
                       diag.stringPool()
                           ? diag.stringPool()->lookupView(name)
                           : std::string_view{"<unknown>"},
                       "'; the existing declaration is a ",
                       symbolKindName(existing->kind));
            return false;
        }

        // ─── Add ───────────────────────────────────────────────────────────
        m_symbols.push_back(Symbol{name, kind, decl});
        return true;
    }

    const Symbol *SymbolTable::find(InternedString name) const noexcept
    {
        for (const Symbol &s : m_symbols)
        {
            if (s.name == name)
            {
                return &s;
            }
        }
        return nullptr;
    }

} // namespace lucid::sema
