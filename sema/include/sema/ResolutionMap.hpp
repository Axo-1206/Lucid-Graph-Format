/// @file sema/ResolutionMap.hpp
///
/// @brief The result of Pass 2: what every reference resolved to.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A map from reference AST nodes to the declarations they name. The
/// reference node is an IdentifierValueAST, a FieldAccessValueAST, a
/// TypeIdAST, a NodeExprAST's type, or a trigger name's node — any
/// place in the AST where a name refers to something else.
///
/// The target is the declaration node that introduces the name. It is
/// not always a DeclAST:
/// a few other reference kinds resolve to non-DeclAST targets. The
/// value type is BaseAST* so the map covers every case.
///
/// ─── Three states ─────────────────────────────────────────────────────────
/// A reference has one of three states in the map:
///
///   - Not recorded. The resolver did not visit it. This is a bug;
///     every reference is visited.
///   - Recorded with a non-null target. The reference resolved to a
///     declaration.
///   - Recorded with a null target. The reference was deferred. It is
///     used for references to imported modules that have not yet been
///     loaded. Step 7.8 fills these in.
///
/// ─── Why a separate map and not AST fields ────────────────────────────────
/// The AST is deliberately syntactic. Adding resolution pointers to it
/// would mix passes' concerns and force Sema to clear and re-populate
/// them on every run. The map keeps the AST untouched and stores each
/// pass's findings in its own structure.
///
/// ─── Lifetime ─────────────────────────────────────────────────────────────
/// The map is owned by the compile session. It lives for the duration
/// of Sema's run and is discarded afterwards; the Graph is the final
/// output, and it does not reference the map.

#pragma once

#include "core/ast/BaseAST.hpp"

#include <cstddef>
#include <unordered_map>

namespace lucid::sema
{

    /// @brief A map from reference nodes to their resolved targets.
    class ResolutionMap
    {
    public:
        ResolutionMap() = default;

        // ─── Building ──────────────────────────────────────────────────────

        /// @brief Record that `ref` resolved to `target`.
        ///
        /// `target` may be null, meaning the reference was deferred
        /// (typically, a reference to an imported module that has not
        /// been loaded yet).
        ///
        /// If `ref` was already recorded, the new entry overwrites the
        /// old one. This lets Step 7.8 overwrite deferred entries with
        /// proper resolutions.
        void record(const BaseAST *ref, const BaseAST *target)
        {
            m_map[ref] = target;
        }

        // ─── Query ─────────────────────────────────────────────────────────

        /// @brief Look up the target for a reference.
        ///
        /// Returns nullptr if the reference was not recorded or was
        /// recorded with a null target. The two cases are
        /// indistinguishable at the call site; both mean "no target
        /// available."
        const BaseAST *lookup(const BaseAST *ref) const noexcept
        {
            auto it = m_map.find(ref);
            if (it == m_map.end())
            {
                return nullptr;
            }
            return it->second;
        }

        /// @brief True if `ref` was recorded, regardless of target.
        bool contains(const BaseAST *ref) const noexcept
        {
            return m_map.find(ref) != m_map.end();
        }

        // ─── Size ──────────────────────────────────────────────────────────

        size_t size() const noexcept { return m_map.size(); }
        bool empty() const noexcept { return m_map.empty(); }

        void clear() noexcept { m_map.clear(); }

    private:
        std::unordered_map<const BaseAST *, const BaseAST *> m_map;
    };

} // namespace lucid::sema
