/// @file sema/TypeMap.hpp
///
/// @brief The result of Pass 3: what type every expression has.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A map from expression AST nodes to their resolved TypeId. Every
/// expression the type checker visits is recorded here.
///
/// An expression has one of three states:
///   - Recorded with a valid TypeId. The expression has that type.
///   - Recorded with an invalid TypeId (Kind::Invalid). The expression
///     is not a value (a resource reference, a trigger node reference,
///     a type name used in a type position).
///   - Not recorded. The type checker did not visit it. This is a bug.
///
/// ─── Why not AST fields ───────────────────────────────────────────────────
/// Same reasoning as ResolutionMap: the AST is syntactic; type
/// information is Pass 3's output and lives in its own structure.

#pragma once

#include "core/ast/BaseAST.hpp"
#include "sema/TypeId.hpp"

#include <cstddef>
#include <unordered_map>

namespace lucid::sema
{

    /// @brief A map from AST nodes to their types.
    class TypeMap
    {
    public:
        TypeMap() = default;

        // ─── Building ──────────────────────────────────────────────────────

        /// Record that `node` has type `type`. If the node was already
        /// recorded, the new type overwrites the old one.
        void record(const BaseAST* node, TypeId type)
        {
            m_map[node] = type;
        }

        // ─── Query ─────────────────────────────────────────────────────────

        /// Look up `node`'s type. Returns an invalid TypeId if the node
        /// was not recorded or was recorded as non-value.
        TypeId lookup(const BaseAST* node) const noexcept
        {
            auto it = m_map.find(node);
            if (it == m_map.end())
            {
                return TypeId{};
            }
            return it->second;
        }

        bool contains(const BaseAST* node) const noexcept
        {
            return m_map.find(node) != m_map.end();
        }

        size_t size() const noexcept { return m_map.size(); }
        bool empty() const noexcept { return m_map.empty(); }
        void clear() noexcept { m_map.clear(); }

    private:
        std::unordered_map<const BaseAST*, TypeId> m_map;
    };

} // namespace lucid::sema
