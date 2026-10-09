/// @file serialization/src/serialization/Writer.hpp
///
/// @brief The low-level byte writer used by the serializer.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A `Writer` wraps a `std::vector<uint8_t>&` and appends fixed-width
/// little-endian integers and raw byte runs. Every multi-byte integer
/// is written low byte first. No alignment padding, no endianness
/// marker, no varints.
///
/// ─── Why not just memcpy ──────────────────────────────────────────────────
/// `memcpy` of a `uint32_t` writes the platform's native byte order.
/// That works on x86-64 and ARM, but it silently breaks on a big-endian
/// platform. Writing the four bytes explicitly is a few extra
/// instructions and makes the format's definition ("little-endian") the
/// code's behavior, not an accident of the host.
///
/// ─── Floating point ───────────────────────────────────────────────────────
/// `float` and `double` are written as their IEEE 754 bit patterns,
/// little-endian. `std::memcpy` from the value into a `uint32_t` /
/// `uint64_t` obtains the bit pattern without aliasing UB, then the
/// integer writer emits it.

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace lucid::serialization::detail
{

    class Writer
    {
    public:
        explicit Writer(std::vector<uint8_t> &out) noexcept : m_out(out) {}

        // ─── Sizes ─────────────────────────────────────────────────────────

        size_t size() const noexcept { return m_out.size(); }

        // ─── Integer writers ───────────────────────────────────────────────

        void u8(uint8_t v)
        {
            m_out.push_back(v);
        }

        void u16(uint16_t v)
        {
            m_out.push_back(static_cast<uint8_t>(v & 0xFF));
            m_out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        }

        void u32(uint32_t v)
        {
            m_out.push_back(static_cast<uint8_t>(v & 0xFF));
            m_out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
            m_out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
            m_out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        }

        void u64(uint64_t v)
        {
            u32(static_cast<uint32_t>(v & 0xFFFFFFFFu));
            u32(static_cast<uint32_t>((v >> 32) & 0xFFFFFFFFu));
        }

        void i8(int8_t v) { u8(static_cast<uint8_t>(v)); }
        void i16(int16_t v) { u16(static_cast<uint16_t>(v)); }
        void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
        void i64(int64_t v) { u64(static_cast<uint64_t>(v)); }

        // ─── Floating-point writers ────────────────────────────────────────

        void f32(float v)
        {
            uint32_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            u32(bits);
        }

        void f64(double v)
        {
            uint64_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            u64(bits);
        }

        // ─── Byte runs ─────────────────────────────────────────────────────

        void bytes(const void *data, size_t count)
        {
            const auto *p = static_cast<const uint8_t *>(data);
            m_out.insert(m_out.end(), p, p + count);
        }

        void bytes(const std::vector<uint8_t> &v)
        {
            m_out.insert(m_out.end(), v.begin(), v.end());
        }

        // ─── Patch a 4-byte field in place ─────────────────────────────────
        //
        // Used to write the section table before the sections are
        // written: each entry's `section_size` is filled in after the
        // section is built. The offset is the byte position of the
        // field's first byte in the output vector.

        void patchU32(size_t offset, uint32_t v)
        {
            m_out[offset + 0] = static_cast<uint8_t>(v & 0xFF);
            m_out[offset + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
            m_out[offset + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
            m_out[offset + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
        }

    private:
        std::vector<uint8_t> &m_out;
    };

} // namespace lucid::serialization::detail