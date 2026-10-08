/// @file sema/Primitives.hpp
///
/// @brief The primitive types and their aliases.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// The canonical list of primitive type names and the alias table.
///
/// Canonical names (grammar §2.9):
///   bool, char, string,
///   int8, int16, int32, int64,
///   uint8, uint16, uint32, uint64,
///   float32, float64
///
/// Aliases (conveniences for the script writer):
///   float  → float32
///   double → float64
///   int    → int32
///   uint   → uint32
///   byte   → uint8
///   short  → int16
///   long   → int64
///
/// ─── Why aliases are here, not in the registry ────────────────────────────
/// Aliases are a grammar-level convenience. They are the same for every
/// engine. The registry declares engine-specific things (node types,
/// enum types, handle types); it does not declare primitives.
///
/// If a future engine wants custom aliases, a future registry extension
/// adds an alias table there.

#pragma once

#include "sema/Literal.hpp"

#include <string_view>

namespace lucid::sema
{

    /// @brief If `name` is a primitive alias, return its canonical name.
    /// Otherwise, return `name` unchanged.
    ///
    /// The canonical names themselves are idempotent: passing "float32"
    /// returns "float32".
    inline std::string_view normalizePrimitiveName(std::string_view name) noexcept
    {
        if (name == "float")  return "float32";
        if (name == "double") return "float64";
        if (name == "int")    return "int32";
        if (name == "uint")   return "uint32";
        if (name == "byte")   return "uint8";
        if (name == "short")  return "int16";
        if (name == "long")   return "int64";
        return name;
    }

    /// @brief True if `name` is a canonical primitive type name.
    inline bool isCanonicalPrimitive(std::string_view name) noexcept
    {
        return name == "bool"    || name == "char"    || name == "string" ||
               name == "int8"    || name == "int16"   || name == "int32" ||
               name == "int64"   ||
               name == "uint8"   || name == "uint16"  || name == "uint32" ||
               name == "uint64"  ||
               name == "float32" || name == "float64";
    }

    /// @brief Map a canonical primitive name to the corresponding
    ///        Literal::Kind.
    ///
    /// Returns Literal::Kind::Nil if `name` is not a canonical primitive.
    inline Literal::Kind primitiveLiteralKind(std::string_view name) noexcept
    {
        if (name == "bool")    return Literal::Kind::Bool;
        if (name == "char")    return Literal::Kind::Char;
        if (name == "string")  return Literal::Kind::String;
        if (name == "int8")    return Literal::Kind::Int8;
        if (name == "int16")   return Literal::Kind::Int16;
        if (name == "int32")   return Literal::Kind::Int32;
        if (name == "int64")   return Literal::Kind::Int64;
        if (name == "uint8")   return Literal::Kind::UInt8;
        if (name == "uint16")  return Literal::Kind::UInt16;
        if (name == "uint32")  return Literal::Kind::UInt32;
        if (name == "uint64")  return Literal::Kind::UInt64;
        if (name == "float32") return Literal::Kind::Float32;
        if (name == "float64") return Literal::Kind::Float64;
        return Literal::Kind::Nil;
    }

} // namespace lucid::sema
