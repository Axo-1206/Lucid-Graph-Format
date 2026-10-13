/// @file sema/src/sema/TypeId.cpp
///
/// @brief Implementation of zeroLiteralOf.

#include "sema/TypeId.hpp"

#include "sema/Literal.hpp"
#include "sema/Registry.hpp"

#include <string_view>

namespace lucid::sema
{

    namespace
    {

        /// Find the enum member whose value is 0. Returns nullptr if
        /// the enum has no such member.
        const EnumMemberInfo *
        findZeroMember(const EnumTypeInfo &info) noexcept
        {
            for (const EnumMemberInfo &m : info.members)
            {
                if (m.value == 0)
                    return &m;
            }
            return nullptr;
        }

    } // namespace

    Literal zeroLiteralOf(TypeId type, const Registry &registry) noexcept
    {
        switch (type.kind)
        {
        case TypeId::Kind::Invalid:
            return Literal{};

        case TypeId::Kind::Primitive:
        {
            const std::string_view name = type.name;
            if (name == "bool")
                return makeBool(false);
            if (name == "char")
                return makeChar('\0');
            if (name == "string")
                return Literal::makeString(0, 0);
            if (name == "int8")
                return makeInt8(0);
            if (name == "int16")
                return makeInt16(0);
            if (name == "int32")
                return makeInt32(0);
            if (name == "int64")
                return makeInt64(0);
            if (name == "uint8")
                return makeUInt8(0);
            if (name == "uint16")
                return makeUInt16(0);
            if (name == "uint32")
                return makeUInt32(0);
            if (name == "uint64")
                return makeUInt64(0);
            if (name == "float32")
                return makeFloat32(0.0f);
            if (name == "float64")
                return makeFloat64(0.0);
            return Literal{};
        }

        case TypeId::Kind::Enum:
        {
            // Find the enum in the registry, then find the member
            // whose value is 0.
            for (const EnumTypeInfo &info : registry.enums)
            {
                if (info.name != type.name)
                    continue;

                const EnumMemberInfo *zero = findZeroMember(info);
                if (zero == nullptr)
                {
                    // The registry declares an enum with no zero
                    // member. Sema should have reported this when it
                    // validated the registry; the caller does not
                    // expect to reach here. Return nil so the caller
                    // has a defined value.
                    return Literal{};
                }

                // The member's value is an integer. Its literal kind
                // is Int64 for the general case; the caller can
                // narrow when it knows the enum's declared width.
                // Int64 is the safe choice: it holds every value
                // int64_t can, and the runtime can downcast if it
                // needs to.
                return makeInt64(zero->value);
            }

            // The type names an enum that the registry does not
            // declare. Sema should have reported this as an unknown
            // type. Return nil so the caller has a defined value.
            return Literal{};
        }

        case TypeId::Kind::Handle:
            // A handle's zero is nil.
            return Literal::makeNil();

        case TypeId::Kind::Struct:
            // A struct's zero is resolved by the graph builder, which
            // has the registry (to look up the struct's fields) and
            // the graph's struct-value pool (to write the bytes to).
            // `zeroLiteralOf` returns an unresolved literal; the
            // graph builder substitutes a real struct value.
            //
            // In practice this branch is not reached: the graph
            // builder checks the type's kind before calling
            // `zeroLiteralOf` and dispatches structs to
            // `zeroStructValue`.
            return Literal{};
        }

        return Literal{};
    }

} // namespace lucid::sema