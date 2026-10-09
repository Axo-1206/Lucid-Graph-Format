/// @file sema/ConstantValueMap.hpp
///
/// @brief The result of constant folding: what compile-time value
///        every reducible expression has.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A map from expression AST nodes to their compile-time-known Literal
/// values. It records two kinds of expression:
///
///   - A LiteralValueAST — the literal itself.
///   - A FieldAccessValueAST on an enum member — the member's integer,
///     taken from the registry.
///
/// The map is populated during Pass 3 (type checking). Graph
/// construction reads it to build Arg::Literal entries.
///
/// ─── Why a separate map and not AST fields ────────────────────────────────
/// Same reasoning as TypeMap: the AST is syntactic; constant values are
/// Pass 3's output. The map is a side table, discarded after Sema's run
/// (the Graph is the final output, and it holds its own Literal values).
///
/// ─── The relationship to TypeMap ──────────────────────────────────────────
/// A LiteralValueAST has both a type and a value. TypeMap stores the
/// type (int32 for an integer literal); ConstantValueMap stores the
/// value (the literal's text as an integer). The two maps are
/// independent; a consumer that needs both looks up the same AST node
/// in each.

#pragma once

#include "core/ast/BaseAST.hpp"
#include "sema/Literal.hpp"

#include <cstddef>
#include <unordered_map>

namespace lucid::sema
{

    /// @brief A map from AST nodes to their constant values.
    class ConstantValueMap
    {
    public:
        ConstantValueMap() = default;

        // ─── Building ──────────────────────────────────────────────────────

        /// Record that `node` has the constant value `lit`. If the node
        /// was already recorded, the new value overwrites the old one.
        void record(const BaseAST *node, Literal lit)
        {
            m_map[node] = lit;
        }

        // ─── Query ─────────────────────────────────────────────────────────

        /// Look up `node`'s constant value. Returns nullptr if the node
        /// has no recorded constant (it is not reducible, or was not
        /// visited).
        const Literal *lookup(const BaseAST *node) const noexcept
        {
            auto it = m_map.find(node);
            if (it == m_map.end())
            {
                return nullptr;
            }
            return &it->second;
        }

        bool contains(const BaseAST *node) const noexcept
        {
            return m_map.find(node) != m_map.end();
        }

        // ─── Size ──────────────────────────────────────────────────────────

        size_t size() const noexcept { return m_map.size(); }
        bool empty() const noexcept { return m_map.empty(); }
        void clear() noexcept { m_map.clear(); }

    private:
        std::unordered_map<const BaseAST *, Literal> m_map;
    };

} // namespace lucid::sema