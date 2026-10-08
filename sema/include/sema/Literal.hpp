/// @file sema/Literal.hpp
///
/// @brief A compile-time-known value.
///
/// ─── What a Literal is ────────────────────────────────────────────────────
/// A value that Sema resolves at compile time. It appears in three
/// places in the graph:
///
///   - As a node argument, when the argument is a literal.
///   - As a resource field's default value.
///   - As a resolved enum member reference (the member's integer value).
///
/// ─── The kinds ────────────────────────────────────────────────────────────
/// The kind is one of:
///
///   Nil       — the grammar's `nil` literal. Valid for handle types.
///   Bool      — `true` or `false`.
///   Char      — a single character.
///   String    — a string, stored as (offset, length) into the graph's
///               string pool.
///   Int8..Int64, UInt8..UInt64 — the sized integer types.
///   Float32, Float64          — the sized float types.
///
/// ─── Why the sized kinds, not one `Int` and one `Float`? ─────────────────
/// The registry's TypeId distinguishes `int8` from `int64`. A Literal
/// carries the specific kind so the graph matches the port's type
/// exactly. The union stores every integer as int64_t and every float
/// as double; the kind tells the engine which to narrow to.
///
/// ─── The string case ──────────────────────────────────────────────────────
/// A string's bytes live in the graph's string pool. The Literal
/// stores an offset and a length. The bytes are not owned by the
/// Literal; they are owned by the Graph.
#pragma once

#include <cstdint>

namespace lucid::sema
{

    /// @brief A compile-time-known value.
    struct Literal
    {
        enum class Kind : uint8_t
        {
            Nil,
            Bool,
            Char,
            String,
            Int8,  Int16,  Int32,  Int64,
            UInt8, UInt16, UInt32, UInt64,
            Float32, Float64,
        };

        /// Which kind of literal this is. Determines which union member
        /// is valid.
        Kind kind = Kind::Nil;

        /// The value.
        ///
        /// For Bool:    `b`
        /// For Char:    `c`
        /// For Int*:    `i` (sign-extended into int64_t)
        /// For UInt*:   `u`
        /// For Float*:  `f` (the value, as double)
        /// For String:  `string` (offset and length into the graph's
        ///              string pool)
        /// For Nil:     unused
        union
        {
            bool     b;
            char     c;
            int64_t  i;
            uint64_t u;
            double   f;
            struct
            {
                uint32_t offset;
                uint32_t length;
            } string;
        };

        // ─── Construction ──────────────────────────────────────────────────

        Literal() : kind(Kind::Nil), u(0) {}

        explicit Literal(bool value) : kind(Kind::Bool), b(value) {}

        explicit Literal(char value) : kind(Kind::Char), c(value) {}

        static Literal makeNil()
        {
            return Literal{};
        }

        static Literal makeString(uint32_t offset, uint32_t length)
        {
            Literal lit;
            lit.kind          = Kind::String;
            lit.string.offset = offset;
            lit.string.length = length;
            return lit;
        }

        // ─── Queries ───────────────────────────────────────────────────────

        bool isNil() const noexcept { return kind == Kind::Nil; }

        bool isInteger() const noexcept
        {
            return kind == Kind::Int8  || kind == Kind::Int16  ||
                   kind == Kind::Int32 || kind == Kind::Int64  ||
                   kind == Kind::UInt8 || kind == Kind::UInt16 ||
                   kind == Kind::UInt32 || kind == Kind::UInt64;
        }

        bool isFloat() const noexcept
        {
            return kind == Kind::Float32 || kind == Kind::Float64;
        }

        bool isString() const noexcept { return kind == Kind::String; }
    };

    /// @brief The name of a Literal::Kind, for diagnostics.
    inline const char* literalKindName(Literal::Kind k) noexcept
    {
        switch (k)
        {
        case Literal::Kind::Nil:     return "Nil";
        case Literal::Kind::Bool:    return "Bool";
        case Literal::Kind::Char:    return "Char";
        case Literal::Kind::String:  return "String";
        case Literal::Kind::Int8:    return "Int8";
        case Literal::Kind::Int16:   return "Int16";
        case Literal::Kind::Int32:   return "Int32";
        case Literal::Kind::Int64:   return "Int64";
        case Literal::Kind::UInt8:   return "UInt8";
        case Literal::Kind::UInt16:  return "UInt16";
        case Literal::Kind::UInt32:  return "UInt32";
        case Literal::Kind::UInt64:  return "UInt64";
        case Literal::Kind::Float32: return "Float32";
        case Literal::Kind::Float64: return "Float64";
        }
        return "Unknown";
    }

} // namespace lucid::sema
