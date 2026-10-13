/// @file sema/TypeId.hpp
///
/// @brief A reference to a type, in the registry's vocabulary.
///
/// ─── What a TypeId is ─────────────────────────────────────────────────────
/// A type reference. It is one of three kinds:
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

#include "sema/Literal.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace lucid::sema
{
    struct Registry; // fwd decl for zeroLiteralOf

    /// @brief A tagged reference to a type.
    struct TypeId
    {
        enum class Kind : uint8_t
        {
            Invalid, // default-constructed; not a valid type
            Primitive,
            Enum,
            Handle,
            Struct,
        };

        /// Which category the type belongs to.
        Kind kind = Kind::Invalid;

        /// The type's canonical name. For primitives, this is the
        /// canonical spelling (`float32`, not `float`). For enums and
        /// handles, this is the host-declared name. The name is owned so
        /// TypeIds remain valid after the compilation session ends.
        std::string name;

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

        static TypeId structType(std::string_view n)
        {
            return TypeId{Kind::Struct, n};
        }

        // ─── Queries ───────────────────────────────────────────────────────

        bool isValid() const noexcept { return kind != Kind::Invalid; }
        bool isPrimitive() const noexcept { return kind == Kind::Primitive; }
        bool isEnum() const noexcept { return kind == Kind::Enum; }
        bool isHandle() const noexcept { return kind == Kind::Handle; }
        bool isStruct() const noexcept { return kind == Kind::Struct; }

        /// True if this is a value type. Every kind except Invalid is a
        /// value type: a primitive is a value, an enum member is a value,
        /// and a handle is a value.
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
        case TypeId::Kind::Struct:
            return "Struct";
        }
        return "Unknown";
    }

    // ─── zeroLiteralOf ────────────────────────────────────────────────────────
    //
    // The zero value of a type, as a Literal. Used to resolve a resource
    // field whose source did not write `= value`. Every resource field has
    // a value in the compiled graph; this function supplies the value for
    // the fields where the source omitted it.
    //
    // ─── Definition of "zero" ─────────────────────────────────────────────
    //   bool      → false
    //   char      → '\0'
    //   string    → empty string (offset 0, length 0)
    //   intN      → 0
    //   uintN     → 0
    //   floatN    → 0.0
    //   enum      → the member whose integer value is 0
    //   handle    → nil
    //   Invalid   → nil
    //
    // For an enum, the zero is the member whose value is 0. If the
    // registry declares an enum with no zero member, the function returns
    // a nil literal and the caller (Sema) is expected to have reported
    // the registry inconsistency already. See the note on registry
    // validation in `Registry.hpp`.
    //
    // For a handle, the zero is nil; the graph represents it as a
    // Literal of kind Nil. The runtime resolves a Nil handle to "no
    // resource", which is what a field with no written default should
    // have.
    //
    // For an Invalid type, the function returns a nil literal. Callers
    // should not invoke it with an Invalid type; Sema reports an error
    // for a field with an unresolved type, and this function is not
    // called in that case.

    Literal zeroLiteralOf(TypeId type, const Registry &registry) noexcept;

} // namespace lucid::sema