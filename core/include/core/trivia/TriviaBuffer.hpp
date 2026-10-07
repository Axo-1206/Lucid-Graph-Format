/// @file core/trivia/TriviaBuffer.hpp
///
/// @brief A sorted list of comments from one source file.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A container of Trivia entries, in source order. The lexer appends
/// entries as it encounters them, so the buffer is sorted by construction.
/// Consumers walk the buffer by source location.
///
/// ─── The formatter's usage ────────────────────────────────────────────────
/// The formatter walks the AST in pre-order and drains trivia before each
/// node:
///
///     size_t cursor = 0;
///     for (each node in pre-order) {
///         while (cursor < buffer.size() &&
///                buffer[cursor].loc < node.loc) {
///             emit(buffer[cursor]);
///             ++cursor;
///         }
///         emit(node);
///     }
///
/// The lowerBound method is a convenience for this pattern.
///
/// ─── No ownership beyond the buffer ───────────────────────────────────────
/// The buffer owns its entries. The entries' texts are interned strings
/// owned by a StringPool. The buffer does not own the pool. Lifetime of
/// the buffer is independent of the AST and the token stream.

#pragma once

#include "core/SourceLocation.hpp"
#include "core/trivia/Trivia.hpp"

#include <cstddef>
#include <vector>

namespace lucid::trivia
{

    /// @brief A sorted list of Trivia, in source order.
    class TriviaBuffer
    {
    public:
        TriviaBuffer() = default;

        // Not copyable through a pointer, but copyable by value. The
        // entries are trivially copyable; a copy is fine.
        TriviaBuffer(const TriviaBuffer &) = default;
        TriviaBuffer &operator=(const TriviaBuffer &) = default;
        TriviaBuffer(TriviaBuffer &&) = default;
        TriviaBuffer &operator=(TriviaBuffer &&) = default;

        // ─── Building ──────────────────────────────────────────────────────

        /// Append a comment. Callers append in source order; the buffer
        /// does not re-sort.
        void add(Trivia t) { m_entries.push_back(t); }

        /// Reserve space for `n` entries. Optional; useful when the
        /// number of comments is known.
        void reserve(size_t n) { m_entries.reserve(n); }

        // ─── Query ─────────────────────────────────────────────────────────

        const std::vector<Trivia> &all() const noexcept { return m_entries; }

        size_t size() const noexcept { return m_entries.size(); }
        bool empty() const noexcept { return m_entries.empty(); }

        const Trivia &operator[](size_t i) const noexcept { return m_entries[i]; }

        const Trivia &front() const noexcept { return m_entries.front(); }
        const Trivia &back() const noexcept { return m_entries.back(); }

        // ─── Range query ───────────────────────────────────────────────────

        /// @brief The index of the first entry whose location is at or
        ///        after `loc`.
        ///
        /// Returns `size()` if all entries have locations strictly
        /// before `loc`. Entries at indices `< lowerBound(loc)` have
        /// locations strictly before `loc`.
        ///
        /// Used by the formatter to drain trivia before a node. The
        /// formatter keeps a cursor; when it encounters a node at
        /// location L, it advances the cursor up to `lowerBound(L)`.
        size_t lowerBound(SourceLocation loc) const noexcept
        {
            size_t lo = 0;
            size_t hi = m_entries.size();
            while (lo < hi)
            {
                const size_t mid = lo + (hi - lo) / 2;
                if (m_entries[mid].loc.value < loc.value)
                {
                    lo = mid + 1;
                }
                else
                {
                    hi = mid;
                }
            }
            return lo;
        }

    private:
        std::vector<Trivia> m_entries;
    };

} // namespace lucid::trivia