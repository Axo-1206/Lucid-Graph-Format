/// @file serialization/src/serialization/Reader.hpp
///
/// @brief The low-level byte reader used by the deserializer.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A `Reader` wraps a `ByteSpan` (the file's bytes) plus a cursor. It
/// reads fixed-width little-endian integers and raw byte runs.
///
/// ─── The contract ─────────────────────────────────────────────────────────
/// Every read advances the cursor. A read that would go past the end
/// sets an `overflowed` flag and returns zero; the caller checks the
/// flag once at the end of a section rather than after every read.
/// This keeps the reader readable and puts the bounds check in one
/// place.
///
/// ─── Why not throw ────────────────────────────────────────────────────────
/// The deserializer reports errors through `DiagnosticEngine`, not
/// exceptions. A flag is the lowest-overhead way to record "the file
/// is truncated" and continue reading zeros until the section ends.
/// The diagnostics are then reported against the section that
/// overflowed, not against the first byte past the end.

#pragma once

#include "serialization/ByteSpan.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace lucid::serialization::detail
{

    class Reader
    {
    public:
        explicit Reader(ByteSpan data) noexcept : m_data(data) {}

        // ─── Cursor ────────────────────────────────────────────────────────

        size_t position() const noexcept { return m_pos; }
        size_t remaining() const noexcept { return m_data.size() - m_pos; }
        bool atEnd() const noexcept { return m_pos >= m_data.size(); }

        /// True if any read has gone past the end.
        bool overflowed() const noexcept { return m_overflowed; }

        /// Clear the overflow flag. Used at a section boundary so that
        /// a later section can be read.
        void clearOverflow() noexcept { m_overflowed = false; }

        // ─── Skip ──────────────────────────────────────────────────────────

        void skip(size_t n) noexcept
        {
            if (m_pos + n > m_data.size())
            {
                m_overflowed = true;
                m_pos = m_data.size();
                return;
            }
            m_pos += n;
        }

        // ─── Integer readers ───────────────────────────────────────────────

        uint8_t u8() noexcept
        {
            if (m_pos + 1 > m_data.size())
            {
                m_overflowed = true;
                return 0;
            }
            return m_data[m_pos++];
        }

        uint16_t u16() noexcept
        {
            if (m_pos + 2 > m_data.size())
            {
                m_overflowed = true;
                return 0;
            }
            const uint16_t v =
                static_cast<uint16_t>(m_data[m_pos]) | static_cast<uint16_t>(m_data[m_pos + 1]) << 8;
            m_pos += 2;
            return v;
        }

        uint32_t u32() noexcept
        {
            if (m_pos + 4 > m_data.size())
            {
                m_overflowed = true;
                return 0;
            }
            const uint32_t v =
                static_cast<uint32_t>(m_data[m_pos]) | static_cast<uint32_t>(m_data[m_pos + 1]) << 8 | static_cast<uint32_t>(m_data[m_pos + 2]) << 16 | static_cast<uint32_t>(m_data[m_pos + 3]) << 24;
            m_pos += 4;
            return v;
        }

        uint64_t u64() noexcept
        {
            const uint64_t lo = u32();
            const uint64_t hi = u32();
            return lo | (hi << 32);
        }

        int8_t i8() noexcept { return static_cast<int8_t>(u8()); }
        int16_t i16() noexcept { return static_cast<int16_t>(u16()); }
        int32_t i32() noexcept { return static_cast<int32_t>(u32()); }
        int64_t i64() noexcept { return static_cast<int64_t>(u64()); }

        // ─── Floating-point readers ────────────────────────────────────────

        float f32() noexcept
        {
            const uint32_t bits = u32();
            float v = 0.0f;
            std::memcpy(&v, &bits, sizeof(v));
            return v;
        }

        double f64() noexcept
        {
            const uint64_t bits = u64();
            double v = 0.0;
            std::memcpy(&v, &bits, sizeof(v));
            return v;
        }

        // ─── Byte runs ─────────────────────────────────────────────────────

        /// Copy `count` bytes into `out`. Appends.
        void bytes(std::vector<uint8_t> &out, size_t count) noexcept
        {
            appendBytes(out, count);
        }

        /// Copy `count` bytes into a `std::vector<char>` string pool.
        void bytes(std::vector<char> &out, size_t count) noexcept
        {
            appendBytes(out, count);
        }

    private:
        template <typename ByteContainer>
        void appendBytes(ByteContainer &out, size_t count) noexcept
        {
            if (m_pos + count > m_data.size())
            {
                m_overflowed = true;
                count = m_data.size() - m_pos;
            }
            const size_t start = out.size();
            out.resize(start + count);
            if (count != 0)
            {
                std::memcpy(out.data() + start, m_data.data() + m_pos, count);
            }
            m_pos += count;
        }

    public:
        /// Return a view into the underlying buffer, `count` bytes long,
        /// at the current position, advancing the cursor. The view is
        /// valid for the lifetime of the ByteSpan.
        ByteSpan bytesView(size_t count) noexcept
        {
            if (m_pos + count > m_data.size())
            {
                m_overflowed = true;
                count = m_data.size() - m_pos;
            }
            ByteSpan view = m_data.subspan(m_pos, count);
            m_pos += count;
            return view;
        }

    private:
        ByteSpan m_data;
        size_t m_pos = 0;
        bool m_overflowed = false;
    };

} // namespace lucid::serialization::detail