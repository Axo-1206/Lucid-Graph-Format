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
/// ─── The three states ─────────────────────────────────────────────────────
/// A reference has one of three states in the map:
///
///   - Not recorded. The resolver did not visit it. This is a bug;
///     every reference is visited.
///   - Recorded with a non-null target. The reference resolved to a
///     declaration.
///   - Recorded with a null target. The reference was deferred. It is
///     used for references that the resolver cannot resolve locally:
///     a qualified type through an import, a handle type that lives
///     in the registry, and so on. A later pass fills these in.
///
/// ─── What a target is, per reference kind ─────────────────────────────────
/// The target is the declaration that introduces the name. Which
/// declaration that is depends on the reference:
///
///   IdentifierValueAST
///     A bare name: `player`. The target is the declaration the name
///     refers to, or null if the reference was deferred.
///
///   TypeIdAST
///     A type name: `Key`, `core::Key`. For a local enum or resource,
///     the target is the declaration. For a primitive or handle, the
///     target is null (the registry is checked in Pass 3). For a
///     qualified type, the target is null (a later pass resolves it
///     through the import).
///
///   FieldAccessValueAST
///     A field access: `Config.speed`, `Key.W`, `player_health.current`.
///     The target is the **object's** declaration, not the field's.
///     `Config.speed` records the `ResourceDeclAST` for `Config`;
///     `Key.W` records the `EnumDeclAST` for `Key`. The consumer (the
///     type checker, and later passes) reads the object's kind and
///     resolves the field against it. Recording the object's
///     declaration is deliberate: the field's target has no single
///     AST kind (a resource field, an enum member, and a node output
///     are different things), and the object's declaration is the
///     common piece every consumer already needs.
///
///   Trigger names (in a NodeDeclAST's `on` clause)
///     The `on` clause's targets are stored as a span of interned
///     names, not as individual AST nodes. The resolver records the
///     target node's declaration keyed by the **enclosing node
///     declaration**, not by the trigger name. A node with two
///     triggers records only one entry, and the last one wins. This
///     is a known limitation; see the note in Resolver.cpp.
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
        /// (typically, a reference that a later pass resolves, such as
        /// a qualified type through an import).
        ///
        /// If `ref` was already recorded, the new entry overwrites the
        /// old one. This lets a later pass overwrite a deferred entry
        /// with a proper resolution.
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
        ///
        /// The meaning of the returned pointer depends on the reference
        /// kind. See the file-level doc for what each reference kind
        /// records.
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