/// @file sema/TypeId.hpp
///
/// @brief A reference to a type, in the registry's vocabulary.
///
/// ─── What a TypeId is ─────────────────────────────────────────────────────
/// A type reference. It is one of four kinds:
///
///   Primitive — `bool`, `char`, `string`, `int8`..`int64`,
///               `uint8`..`uint64`, `float32`, `float64`. The name is
///               the canonical spelling (`float32`, not `float`).
///   Enum      — a host-declared enum, like `Key` or `Direction`.
///   Handle    — a host-declared handle, like `BodyRef` or `TextureRef`.
///
/// ─── The name is always present ───────────────────────────────────────────
/// Even for primitives, the name is stored. `TypeId{Primitive, "float32"}`
/// is the canonical form. Sema normalizes aliases (`float` → `float32`)
/// before constructing a TypeId, so the name field is always the
/// canonical spelling.
#pragma once

#include <cstdint>
#include <string_view>

namespace lucid::sema
{

    /// @brief A tagged reference to a type.
    struct TypeId
    {
        enum class Kind : uint8_t
        {
            Invalid, // default-constructed; not a valid type
            Primitive,
            Enum,
            Handle,
            Event,
        };

        /// Which category the type belongs to.
        Kind kind = Kind::Invalid;

        /// The type's canonical name. For primitives, this is the
        /// canonical spelling (`float32`, not `float`). For enums and
        /// handles, this is the host-declared name. For Event, this is
        /// the literal string "Event".
        std::string_view name;

        // ─── Construction ──────────────────────────────────────────────────

        TypeId() = default;

        TypeId(Kind k, std::string_view n) : kind(k), name(n) {}

        // ─── Convenience constructors ──────────────────────────────────────

        static TypeId primitive(std::string_view n)
        {
            return TypeId{Kind::Primitive, n};
        }

        static TypeId enumType(std::string_view n)
        {
            return TypeId{Kind::Enum, n};
        }

        static TypeId handle(std::string_view n)
        {
            return TypeId{Kind::Handle, n};
        }

        static TypeId event()
        {
            return TypeId{Kind::Event, "Event"};
        }

        // ─── Queries ───────────────────────────────────────────────────────

        bool isValid() const noexcept { return kind != Kind::Invalid; }
        bool isPrimitive() const noexcept { return kind == Kind::Primitive; }
        bool isEnum() const noexcept { return kind == Kind::Enum; }
        bool isHandle() const noexcept { return kind == Kind::Handle; }
        bool isEvent() const noexcept { return kind == Kind::Event; }

        /// True if this is a value type: primitive, enum, or handle.
        /// Event is not a value type.
        bool isValue() const noexcept
        {
            return kind == Kind::Primitive ||
                   kind == Kind::Enum ||
                   kind == Kind::Handle;
        }

        // ─── Comparison ────────────────────────────────────────────────────

        bool operator==(const TypeId &other) const noexcept
        {
            return kind == other.kind && name == other.name;
        }

        bool operator!=(const TypeId &other) const noexcept
        {
            return !(*this == other);
        }
    };

    /// @brief The name of a TypeId::Kind, for diagnostics.
    inline const char *typeKindName(TypeId::Kind k) noexcept
    {
        switch (k)
        {
        case TypeId::Kind::Invalid:
            return "Invalid";
        case TypeId::Kind::Primitive:
            return "Primitive";
        case TypeId::Kind::Enum:
            return "Enum";
        case TypeId::Kind::Handle:
            return "Handle";
        case TypeId::Kind::Event:
            return "Event";
        }
        return "Unknown";
    }

} // namespace lucid::sema
