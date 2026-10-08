/// @file sema/src/sema/TypeChecker.cpp
///
/// @brief Implementation of Pass 3: type checking.
///
/// ─── The walk ─────────────────────────────────────────────────────────────
/// One function per AST kind. Each function assigns types to the
/// expressions it contains and recurses into sub-expressions.
///
/// ─── Registry lookup ──────────────────────────────────────────────────────
/// Node types and enum types are looked up in the registry by name.
/// Because the registry's spans are unsorted, a linear scan is used.
/// For typical registries (tens to hundreds of entries), this is fine.

#include "TypeChecker.hpp"

#include "sema/Primitives.hpp"

#include "core/ast/DeclAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/ast/ValueAST.hpp"
#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/StringPool.hpp"

#include <string_view>

using namespace lucid::diag;

namespace lucid::sema
{

    namespace
    {

        // ─── Registry lookups ──────────────────────────────────────────────

        const NodeTypeInfo *lookupNodeType(const Registry &registry,
                                           std::string_view name)
        {
            for (const NodeTypeInfo &info : registry.nodeTypes)
            {
                if (info.name == name)
                    return &info;
            }
            return nullptr;
        }

        const EnumTypeInfo *lookupEnumType(const Registry &registry,
                                           std::string_view name)
        {
            for (const EnumTypeInfo &info : registry.enums)
            {
                if (info.name == name)
                    return &info;
            }
            return nullptr;
        }

        const HandleTypeInfo *lookupHandleType(const Registry &registry,
                                               std::string_view name)
        {
            for (const HandleTypeInfo &info : registry.handles)
            {
                if (info.name == name)
                    return &info;
            }
            return nullptr;
        }

        // ─── Type compatibility ────────────────────────────────────────────

        /// True if an expression of type `value` can be used where a
        /// slot of type `slot` is expected.
        ///
        /// The rules:
        ///   - Exact match on kind and name.
        ///   - `nil` (value.kind == Invalid and value is the nil literal)
        ///     is compatible with any Handle slot.
        ///
        /// The nil case is handled by the caller, which knows whether
        /// the argument is a nil literal. This function only handles
        /// non-nil types.
        bool typesMatch(TypeId value, TypeId slot) noexcept
        {
            return value == slot;
        }

    } // namespace

    // ─── TypeChecker ──────────────────────────────────────────────────────────

    class TypeChecker
    {
    public:
        TypeChecker(const SymbolTable &symbols,
                    const ResolutionMap &resolutions,
                    const Registry &registry,
                    TypeMap &types,
                    DiagnosticEngine &diag)
            : m_symbols(symbols), m_resolutions(resolutions), m_registry(registry), m_types(types), m_diag(diag)
        {
        }

        void checkModule(const ModuleAST *module);

    private:
        // ─── Declarations ──────────────────────────────────────────────────

        void checkDecl(const DeclAST *decl);
        void checkResourceDecl(const ResourceDeclAST *decl);
        void checkResourceField(const ResourceFieldAST *field);
        void checkNodeDecl(const NodeDeclAST *decl);
        void checkCompositeDecl(const CompositeDeclAST *decl);
        void checkCompositeInput(const CompositeInputAST *input);
        void checkCompositeOutput(const CompositeOutputAST *output);

        // ─── Expressions ───────────────────────────────────────────────────

        TypeId checkValue(const BaseAST *value);
        TypeId checkLiteralValue(const LiteralValueAST *value);
        TypeId checkIdentifierValue(const IdentifierValueAST *value);
        TypeId checkFieldAccessValue(const FieldAccessValueAST *value);
        TypeId checkInlineNodeValue(const InlineNodeValueAST *value);
        TypeId checkNodeExpr(const NodeExprAST *node);

        // ─── Types ─────────────────────────────────────────────────────────

        TypeId checkTypeId(const TypeIdAST *type);

        // ─── Helpers ───────────────────────────────────────────────────────

        std::string_view name(InternedString s) const
        {
            if (m_diag.stringPool() == nullptr)
                return "<unknown>";
            return m_diag.stringPool()->lookupView(s);
        }

        /// True if the value node's type is a value node with a single
        /// output.
        TypeId resultTypeOf(const NodeTypeInfo &info) const
        {
            if (info.kind != NodeKind::Value)
                return TypeId{};
            if (info.outputs.size() != 1)
                return TypeId{};
            return info.outputs[0].type;
        }

        // ─── State ─────────────────────────────────────────────────────────

        const SymbolTable &m_symbols;
        const ResolutionMap &m_resolutions;
        const Registry &m_registry;
        TypeMap &m_types;
        DiagnosticEngine &m_diag;
    };

    // ─── Module ───────────────────────────────────────────────────────────────

    void TypeChecker::checkModule(const ModuleAST *module)
    {
        if (module == nullptr)
            return;
        for (DeclAST *decl : module->decls)
        {
            if (decl == nullptr)
                continue;
            checkDecl(decl);
        }
    }

    // ─── Declarations ─────────────────────────────────────────────────────────

    void TypeChecker::checkDecl(const DeclAST *decl)
    {
        if (decl == nullptr)
            return;

        switch (decl->kind)
        {
        case ASTKind::ResourceDecl:
            checkResourceDecl(decl->as<ResourceDeclAST>());
            break;
        case ASTKind::NodeDecl:
            checkNodeDecl(decl->as<NodeDeclAST>());
            break;
        case ASTKind::CompositeDecl:
            checkCompositeDecl(decl->as<CompositeDeclAST>());
            break;
        default:
            break;
        }
    }

    void TypeChecker::checkResourceDecl(const ResourceDeclAST *decl)
    {
        if (decl == nullptr)
            return;

        for (ResourceFieldAST *field : decl->fields)
        {
            checkResourceField(field);
        }
    }

    void TypeChecker::checkResourceField(const ResourceFieldAST *field)
    {
        if (field == nullptr)
            return;

        // ─── The field's type ──────────────────────────────────────────────
        TypeId fieldType;
        if (field->type != nullptr)
        {
            fieldType = checkTypeId(field->type);
        }

        // The field's type must be a value type. Event is not allowed.
        if (fieldType.isValid() && fieldType.isEvent())
        {
            m_diag.error(DiagCode::Type_InvalidDefault, field,
                         "a resource field cannot have type Event");
            return;
        }

        if (!fieldType.isValid())
        {
            // checkTypeId already reported the error.
            return;
        }

        // ─── The field's default ───────────────────────────────────────────
        if (field->defaultValue == nullptr)
            return;

        const TypeId defaultType = checkLiteralValue(field->defaultValue);

        // The default must match the field's type. nil is allowed for
        // handle types.
        const bool isNil = (defaultType.kind == TypeId::Kind::Invalid &&
                            field->defaultValue->kind == LiteralKind::Nil);

        if (isNil)
        {
            if (!fieldType.isHandle())
            {
                m_diag.error(DiagCode::Type_InvalidDefault, field->defaultValue,
                             "nil is only valid for handle types");
            }
            return;
        }

        if (defaultType != fieldType)
        {
            m_diag.error(DiagCode::Type_InvalidDefault, field->defaultValue,
                         "default value type does not match the field's type");
        }
    }

    void TypeChecker::checkNodeDecl(const NodeDeclAST *decl)
    {
        if (decl == nullptr)
            return;

        if (decl->expr != nullptr)
        {
            checkNodeExpr(decl->expr);
        }
    }

    void TypeChecker::checkCompositeDecl(const CompositeDeclAST *decl)
    {
        if (decl == nullptr)
            return;

        for (CompositeInputAST *input : decl->inputs)
        {
            checkCompositeInput(input);
        }
        for (CompositeOutputAST *output : decl->outputs)
        {
            checkCompositeOutput(output);
        }
        // Composite bodies are checked in Step 7.6.
    }

    void TypeChecker::checkCompositeInput(const CompositeInputAST *input)
    {
        if (input == nullptr)
            return;
        if (input->type == nullptr)
            return;

        const TypeId inputType = checkTypeId(input->type);

        if (inputType.isValid() && inputType.isEvent())
        {
            m_diag.error(DiagCode::Type_UnknownType, input,
                         "a composite input cannot have type Event");
        }
    }

    void TypeChecker::checkCompositeOutput(const CompositeOutputAST *output)
    {
        if (output == nullptr)
            return;
        if (output->type == nullptr)
            return;

        const TypeId outputType = checkTypeId(output->type);
        if (!outputType.isValid())
            return;

        // The output's value must match the output's type.
        if (output->value == nullptr)
            return;

        const TypeId valueType = checkValue(output->value);

        // For Event outputs, the value must resolve to a trigger node.
        // That is checked in Step 7.5; here we only check that the
        // value exists and is a reference.
        if (outputType.isEvent())
        {
            // The value's type is not a value type; no type check here.
            return;
        }

        // nil is allowed for handle types.
        const bool isNil = (valueType.kind == TypeId::Kind::Invalid &&
                            output->value->kind == ASTKind::LiteralValue &&
                            output->value->as<LiteralValueAST>()->kind ==
                                LiteralKind::Nil);

        if (isNil && outputType.isHandle())
            return;

        if (valueType != outputType)
        {
            m_diag.error(DiagCode::Type_InvalidOutputBinding, output->value,
                         "output binding's type does not match the output's "
                         "declared type");
        }
    }

    // ─── Values ───────────────────────────────────────────────────────────────

    TypeId TypeChecker::checkValue(const BaseAST *value)
    {
        if (value == nullptr)
            return TypeId{};

        switch (value->kind)
        {
        case ASTKind::LiteralValue:
            return checkLiteralValue(value->as<LiteralValueAST>());
        case ASTKind::IdentifierValue:
            return checkIdentifierValue(value->as<IdentifierValueAST>());
        case ASTKind::FieldAccessValue:
            return checkFieldAccessValue(value->as<FieldAccessValueAST>());
        case ASTKind::InlineNodeValue:
            return checkInlineNodeValue(value->as<InlineNodeValueAST>());
        default:
            return TypeId{};
        }
    }

    TypeId TypeChecker::checkLiteralValue(const LiteralValueAST *value)
    {
        if (value == nullptr)
            return TypeId{};

        // The literal's type is derived from its kind:
        //   Int     → int32 (default)
        //   Float   → float32 (default)
        //   String  → string
        //   Char    → char
        //   Bool    → bool
        //   Nil     → Invalid (special; allowed only for handles)
        //
        // The graph stores the literal's exact kind; the type checker
        // records the "default" type. Actual port checking matches the
        // literal's kind against the port's type.
        switch (value->kind)
        {
        case LiteralKind::Int:
            m_types.record(value, TypeId::primitive("int32"));
            return TypeId::primitive("int32");
        case LiteralKind::Float:
            m_types.record(value, TypeId::primitive("float32"));
            return TypeId::primitive("float32");
        case LiteralKind::String:
            m_types.record(value, TypeId::primitive("string"));
            return TypeId::primitive("string");
        case LiteralKind::Char:
            m_types.record(value, TypeId::primitive("char"));
            return TypeId::primitive("char");
        case LiteralKind::Bool:
            m_types.record(value, TypeId::primitive("bool"));
            return TypeId::primitive("bool");
        case LiteralKind::Nil:
            // Nil has no type. Record invalid; the caller checks
            // contextually.
            m_types.record(value, TypeId{});
            return TypeId{};
        }
        return TypeId{};
    }

    TypeId TypeChecker::checkIdentifierValue(const IdentifierValueAST *value)
    {
        if (value == nullptr)
            return TypeId{};

        // Look up the resolved target.
        const BaseAST *target = m_resolutions.lookup(value);
        if (target == nullptr)
        {
            // Deferred or unresolved. Pass 2 already reported the
            // error if it was a local name.
            m_types.record(value, TypeId{});
            return TypeId{};
        }

        // The target's kind determines the type of the reference.
        switch (target->kind)
        {
        case ASTKind::NodeDecl:
        {
            const auto *nodeDecl = target->as<NodeDeclAST>();
            // Look up the node type in the registry.
            if (nodeDecl->expr == nullptr || nodeDecl->expr->type == nullptr)
            {
                m_types.record(value, TypeId{});
                return TypeId{};
            }
            const auto *info = lookupNodeType(
                m_registry, name(nodeDecl->expr->type->name));
            if (info == nullptr)
            {
                m_types.record(value, TypeId{});
                return TypeId{};
            }
            // A node reference's type is the node's result type (for
            // value nodes) or invalid (for actions and triggers).
            const TypeId t = resultTypeOf(*info);
            m_types.record(value, t);
            return t;
        }
        case ASTKind::ResourceDecl:
            // A bare resource reference is not a value.
            m_types.record(value, TypeId{});
            return TypeId{};

        case ASTKind::CompositeInput:
            // Composite inputs are not visible at module scope; the
            // composite body (Step 7.6) handles them.
            m_types.record(value, TypeId{});
            return TypeId{};

        case ASTKind::CompositeDecl:
            // A composite reference as a bare identifier: not a value.
            m_types.record(value, TypeId{});
            return TypeId{};

        default:
            m_types.record(value, TypeId{});
            return TypeId{};
        }
    }

    TypeId TypeChecker::checkFieldAccessValue(const FieldAccessValueAST *value)
    {
        if (value == nullptr)
            return TypeId{};

        const BaseAST *target = m_resolutions.lookup(value);
        if (target == nullptr)
        {
            // The object did not resolve (Pass 2 reported it).
            m_types.record(value, TypeId{});
            return TypeId{};
        }

        // The object's kind determines how to resolve the field.
        switch (target->kind)
        {
        case ASTKind::ResourceDecl:
        {
            const auto *res = target->as<ResourceDeclAST>();
            // Find the field.
            for (ResourceFieldAST *field : res->fields)
            {
                if (field != nullptr && field->name == value->field)
                {
                    // The field's type is the result.
                    TypeId fieldType = field->type
                                           ? checkTypeId(field->type)
                                           : TypeId{};
                    m_types.record(value, fieldType);
                    return fieldType;
                }
            }
            m_diag.error(DiagCode::Name_UndefinedField, value,
                         "no field '", name(value->field),
                         "' on resource '", name(res->name), "'");
            m_types.record(value, TypeId{});
            return TypeId{};
        }
        case ASTKind::EnumDecl:
        {
            // An enum field access is `Key.W`. Its type is the enum.
            const auto *enumDecl = target->as<EnumDeclAST>();
            // Verify the member exists. The registry is authoritative;
            // the script's `enum Key { ... }` may be a subset.
            const auto *enumInfo = lookupEnumType(m_registry,
                                                  name(enumDecl->name));
            if (enumInfo == nullptr)
            {
                // The registry does not declare this enum. Deferred to
                // Step 7.5 or reported elsewhere; for now, accept it
                // and record the enum type by name.
                const TypeId enumType =
                    TypeId::enumType(name(enumDecl->name));
                m_types.record(value, enumType);
                return enumType;
            }
            bool found = false;
            for (const EnumMemberInfo &member : enumInfo->members)
            {
                if (member.name == name(value->field))
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                m_diag.error(DiagCode::Name_UndefinedEnumMember, value,
                             "no member '", name(value->field),
                             "' on enum '", name(enumDecl->name), "'");
            }
            const TypeId enumType = TypeId::enumType(name(enumDecl->name));
            m_types.record(value, enumType);
            return enumType;
        }
        default:
            // The object is not a resource or enum at module scope.
            // Composite outputs and imports are handled in later steps.
            m_types.record(value, TypeId{});
            return TypeId{};
        }
    }

    TypeId TypeChecker::checkInlineNodeValue(const InlineNodeValueAST *value)
    {
        if (value == nullptr || value->node == nullptr)
            return TypeId{};
        return checkNodeExpr(value->node);
    }

    TypeId TypeChecker::checkNodeExpr(const NodeExprAST *node)
    {
        if (node == nullptr)
            return TypeId{};

        // ─── Resolve the node type ─────────────────────────────────────────
        if (node->type == nullptr)
        {
            return TypeId{};
        }

        const BaseAST *target = m_resolutions.lookup(node->type);

        // If the resolution points at a composite, the node expression
        // is a composite use. Composite uses are typed by their output
        // in Step 7.6; here we record a placeholder.
        if (target != nullptr && target->kind == ASTKind::CompositeDecl)
        {
            // The composite's use in a value context is handled in
            // Step 7.6. Record invalid for now.
            m_types.record(node, TypeId{});
            return TypeId{};
        }

        // Otherwise, the type name should resolve to a node type in
        // the registry.
        const std::string_view nodeTypeName = name(node->type->name);
        const NodeTypeInfo *info = lookupNodeType(m_registry, nodeTypeName);

        if (info == nullptr)
        {
            m_diag.error(DiagCode::Type_UnknownNodeType, node->type,
                         "unknown node type '", nodeTypeName, "'");
            m_types.record(node, TypeId{});
            return TypeId{};
        }

        // ─── Check argument count ──────────────────────────────────────────
        if (node->args.size() != info->inputs.size())
        {
            m_diag.error(DiagCode::Type_ArgCountMismatch, node,
                         "node type '", nodeTypeName, "' expects ",
                         static_cast<uint64_t>(info->inputs.size()),
                         " argument(s), but ",
                         static_cast<uint64_t>(node->args.size()),
                         " were given");
            // Continue checking; we still record a type.
        }

        // ─── Check argument types ──────────────────────────────────────────
        const size_t count = node->args.size() < info->inputs.size()
                                 ? node->args.size()
                                 : info->inputs.size();

        for (size_t i = 0; i < count; ++i)
        {
            const TypeId argType = checkValue(node->args[i]);
            const TypeId portType = info->inputs[i].type;

            // nil is allowed for handle ports.
            const bool isNil =
                (node->args[i] != nullptr &&
                 node->args[i]->kind == ASTKind::LiteralValue &&
                 node->args[i]->as<LiteralValueAST>()->kind ==
                     LiteralKind::Nil);

            if (isNil)
            {
                if (!portType.isHandle())
                {
                    m_diag.error(DiagCode::Type_Mismatch, node->args[i],
                                 "nil is only valid for handle types");
                }
                continue;
            }

            if (!argType.isValid())
            {
                // The argument has no value type (a resource, an
                // action node, etc.).
                m_diag.error(DiagCode::Type_InvalidNodeArg, node->args[i],
                             "argument is not a valid value");
                continue;
            }

            if (argType != portType)
            {
                m_diag.error(DiagCode::Type_Mismatch, node->args[i],
                             "argument type does not match port type");
            }
        }

        // ─── Check the node's result ───────────────────────────────────────
        const TypeId result = resultTypeOf(*info);
        m_types.record(node, result);
        return result;
    }

    // ─── Types ────────────────────────────────────────────────────────────────

    TypeId TypeChecker::checkTypeId(const TypeIdAST *type)
    {
        if (type == nullptr)
            return TypeId{};

        const std::string_view raw = name(type->name);
        const std::string_view canonical = normalizePrimitiveName(raw);

        // ─── Primitive ─────────────────────────────────────────────────────
        if (isCanonicalPrimitive(canonical))
        {
            const TypeId t = TypeId::primitive(canonical);
            m_types.record(type, t);
            return t;
        }

        // ─── Check the resolution ─────────────────────────────────────────
        const BaseAST *target = m_resolutions.lookup(type);

        if (target != nullptr)
        {
            if (target->kind == ASTKind::EnumDecl)
            {
                const TypeId t = TypeId::enumType(name(type->name));
                m_types.record(type, t);
                return t;
            }
            if (target->kind == ASTKind::ResourceDecl)
            {
                // A resource used as a type is not supported by the
                // grammar's `type_id`. Report it.
                m_diag.error(DiagCode::Type_UnknownType, type,
                             "a resource cannot be used as a type");
                m_types.record(type, TypeId{});
                return TypeId{};
            }
        }

        // ─── Handle ────────────────────────────────────────────────────────
        if (lookupHandleType(m_registry, canonical) != nullptr)
        {
            const TypeId t = TypeId::handle(canonical);
            m_types.record(type, t);
            return t;
        }

        // ─── Event ─────────────────────────────────────────────────────────
        if (raw == "Event")
        {
            const TypeId t = TypeId::event();
            m_types.record(type, t);
            return t;
        }

        // ─── Unknown ───────────────────────────────────────────────────────
        m_diag.error(DiagCode::Type_UnknownType, type,
                     "unknown type '", raw, "'");
        m_types.record(type, TypeId{});
        return TypeId{};
    }

    // ─── Public entry point ───────────────────────────────────────────────────

    void checkTypes(const ModuleAST *module,
                    const SymbolTable &symbols,
                    const ResolutionMap &resolutions,
                    const Registry &registry,
                    TypeMap &types,
                    DiagnosticEngine &diag)
    {
        TypeChecker checker(symbols, resolutions, registry, types, diag);
        checker.checkModule(module);
    }

} // namespace lucid::sema
