/// @file sema/ModuleTable.hpp
///
/// @brief A map from a module's name to its symbol table.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// When a module imports another, the imported module's name (the last
/// segment of its path) becomes available as a `::` qualifier:
/// `keys::Key`. The ModuleTable is the map the resolver uses to look up
/// the module name in a `::`-qualified type reference.
///
/// ─── Why a separate type and not a bare map ───────────────────────────────
/// A wrapper gives the type a name and a `find` method, so the resolver
/// reads `m_moduleTable.find(name)` rather than
/// `m_moduleTable.count(name) ? m_moduleTable[name] : nullptr`. It also
/// gives a place for a future extension (a lookup that reports the
/// module's origin path, for diagnostics).

#pragma once

#include "core/memory/InternedString.hpp"
#include "sema/SymbolTable.hpp"

#include <unordered_map>

namespace lucid::sema
{

    /// @brief A map from module names to symbol tables.
    struct ModuleTable
    {
        std::unordered_map<InternedString, const SymbolTable *> modules;

        /// @brief Find a module's symbol table by name. Returns nullptr
        ///        if the name is not a module.
        const SymbolTable *find(InternedString name) const noexcept
        {
            auto it = modules.find(name);
            return it == modules.end() ? nullptr : it->second;
        }
    };

} // namespace lucid::sema