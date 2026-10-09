/// @file serialization/ByteSpan.hpp
///
/// @brief A read-only view over a contiguous byte buffer.
///
/// ─── Why this exists ──────────────────────────────────────────────────────
/// The design doc writes `deserialize(std::span<const uint8_t>, ...)`.
/// `std::span` is C++20. The project is C++17. This type provides the
/// same shape on C++17; on C++20, it collapses to `std::span` via the
/// alias below, so no call site changes when the standard is bumped.
///
/// ─── The contract ─────────────────────────────────────────────────────────
/// A `ByteSpan` does not own its bytes and does not outlive the buffer
/// it views. Every accessor is const and noexcept. There is no `data()`
/// writable overload and no way to grow or shrink.
///
/// ─── Why not `(const uint8_t*, size_t)` ───────────────────────────────────
/// A two-parameter API is easy to misuse: `(bytes.data(), wrongSize)`
/// compiles. One named type carrying both the pointer and the length
/// makes the size impossible to forget and gives a single place to add
/// bounds checks if they are ever wanted.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(__has_include)
#if __has_include(<span>) && defined(__cpp_lib_span) && \
        __cpp_lib_span >= 202002L
#define LUCID_HAS_STD_SPAN 1
#endif
#endif

#if defined(LUCID_HAS_STD_SPAN)
#include <span>
#endif

namespace lucid::serialization
{

#if defined(LUCID_HAS_STD_SPAN)

    /// On C++20+, `ByteSpan` is `std::span<const uint8_t>`.
    using ByteSpan = std::span<const uint8_t>;

#else

    /// A read-only view over a contiguous run of bytes.
    class ByteSpan
    {
    public:
        ByteSpan() = default;

        ByteSpan(const uint8_t *data, size_t size) noexcept
            : m_data(data), m_size(size)
        {
        }

        /// Construct from any container with `data()` and `size()`.
        /// `std::vector<uint8_t>` and `std::string` both work.
        template <typename Container>
        ByteSpan(const Container &c) noexcept
            : m_data(reinterpret_cast<const uint8_t *>(c.data())),
              m_size(c.size())
        {
        }

        const uint8_t *data() const noexcept { return m_data; }
        size_t size() const noexcept { return m_size; }
        bool empty() const noexcept { return m_size == 0; }

        const uint8_t &operator[](size_t i) const noexcept { return m_data[i]; }

        const uint8_t *begin() const noexcept { return m_data; }
        const uint8_t *end() const noexcept { return m_data + m_size; }

        /// A sub-span starting at `offset`, of `count` bytes. The
        /// caller is responsible for `offset + count <= size()`. No
        /// bounds check; this is a low-level reader's primitive.
        ByteSpan subspan(size_t offset, size_t count) const noexcept
        {
            return ByteSpan(m_data + offset, count);
        }

        /// A sub-span from `offset` to the end.
        ByteSpan subspan(size_t offset) const noexcept
        {
            return ByteSpan(m_data + offset, m_size - offset);
        }

    private:
        const uint8_t *m_data = nullptr;
        size_t m_size = 0;
    };

#endif

} // namespace lucid::serialization