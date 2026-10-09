/// @file cli/src/cli/Registry.cpp
///
/// @brief The default registry's definition.

#include "cli/Registry.hpp"

namespace lucid::cli
{

    const lucid::sema::Registry &defaultRegistry()
    {
        using namespace lucid::sema;

        static const PhaseInfo phases[] = {
            {"update"},
        };

        static const EnumMemberInfo keyMembers[] = {
            {"W", 0},
            {"A", 1},
            {"S", 2},
            {"D", 3},
        };
        static const EnumTypeInfo enums[] = {
            {"Key", ArenaSpan<EnumMemberInfo>(keyMembers, 4)},
        };

        static const HandleTypeInfo handles[] = {
            {"BodyRef"},
        };

        static const NodeArgInfo floatArgs[] = {
            {"value", TypeId::primitive("float32")},
        };
        static const NodeArgInfo printArgs[] = {
            {"value", TypeId::primitive("float32")},
        };
        static const NodeArgInfo moveArgs[] = {
            {"body", TypeId::handle("BodyRef")},
        };
        static const NodeArgInfo damageArgs[] = {
            {"body", TypeId::handle("BodyRef")},
            {"amount", TypeId::primitive("int32")},
        };

        static const NodeTypeInfo nodeTypes[] = {
            {"Float32Node", NodeKind::Value, "Math", 0,
             ArenaSpan<NodeArgInfo>(floatArgs, 1),
             TypeId::primitive("float32")},
            {"PrintNode", NodeKind::Action, "Debug", 0,
             ArenaSpan<NodeArgInfo>(printArgs, 1),
             TypeId{}},
            {"MoveBody", NodeKind::Action, "Physics", 0,
             ArenaSpan<NodeArgInfo>(moveArgs, 1),
             TypeId{}},
            {"Damage", NodeKind::Action, "Physics", 0,
             ArenaSpan<NodeArgInfo>(damageArgs, 2),
             TypeId{}},
            {"EveryFrame", NodeKind::Trigger, "Flow", 0,
             ArenaSpan<NodeArgInfo>{},
             TypeId{}},
        };

        static const Registry reg = []
        {
            Registry r;
            r.phases = ArenaSpan<PhaseInfo>(phases, 1);
            r.enums = ArenaSpan<EnumTypeInfo>(enums, 1);
            r.handles = ArenaSpan<HandleTypeInfo>(handles, 1);
            r.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes, 5);
            return r;
        }();

        return reg;
    }

} // namespace lucid::cli