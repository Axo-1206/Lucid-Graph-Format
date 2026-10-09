/// @file sema/src/sema/TypeChecker.cpp
///
/// @brief Implementation of Pass 3: type checking and the trigger rules.
///
/// ─── The walk ─────────────────────────────────────────────────────────────
/// One function per AST kind. Each function assigns types to the
/// expressions it contains and recurses into sub-expressions.
///
/// ─── Registry lookup ──────────────────────────────────────────────────────
/// Node types and enum types are looked up in the registry by name.
/// Because the registry's spans are unsorted, a linear scan is used.
/// For typical registries (tens to hundreds of entries), this is fine.
///
/// ─── The trigger rules ────────────────────────────────────────────────────
/// Two rules are enforced during the node-declaration walk:
///
///   - An action node must have at least one `on` clause.
///   - An `on` clause's target must resolve to a trigger node.
///
/// Both rules live in checkNodeDecl. They share the NodeTypeInfo lookup
/// with the type checks, so no separate walk is needed.

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

        // ─── Expressions ───────────────────────────────────────────────────

        TypeId checkValue(const BaseAST *value);
        TypeId checkLiteralValue(const LiteralValueAST *value);
        TypeId checkIdentifierValue(const IdentifierValueAST *value);
        TypeId checkFieldAccessValue(const FieldAccessValueAST *value);
        TypeId checkInlineNodeValue(const InlineNodeValueAST *value);
        TypeId checkNodeExpr(const NodeExprAST *node);

        // ─── Types ─────────────────────────────────────────────────────────

        TypeId checkTypeId(const TypeIdAST *type);

        // ─── Trigger rules ─────────────────────────────────────────────────

        /// Rule 2: an action node must have at least one `on` clause.
        void checkActionHasOn(const NodeDeclAST *decl,
                              const NodeTypeInfo *info);

        /// Rule 1: every `on` target must resolve to a trigger node.
        void checkOnTargets(const NodeDeclAST *decl);

        // ─── Helpers ───────────────────────────────────────────────────────

        std::string_view name(InternedString s) const
        {
            if (m_diag.stringPool() == nullptr)
                return "<unknown>";
            return m_diag.stringPool()->lookupView(s);
        }

        /// The NodeTypeInfo of a node declaration's type. Returns
        /// nullptr if the declaration is malformed, the type is
        /// missing, or the type is unknown.
        const NodeTypeInfo *nodeTypeOf(const NodeDeclAST *decl) const
        {
            if (decl == nullptr || decl->expr == nullptr ||
                decl->expr->type == nullptr)
            {
                return nullptr;
            }
            return lookupNodeType(m_registry,
                                  name(decl->expr->type->name));
        }

        /// The result type of a Value node. Returns an invalid TypeId
        /// for Action and Trigger nodes.
        TypeId resultTypeOf(const NodeTypeInfo &info) const
        {
            if (info.kind != NodeKind::Value)
                return TypeId{};
            return info.resultType;
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

        TypeId fieldType;
        if (field->type != nullptr)
        {
            fieldType = checkTypeId(field->type);
        }

        if (!fieldType.isValid())
        {
            return;
        }

        if (field->defaultValue == nullptr)
            return;

        // The default is a general value, not a literal.
        const TypeId defaultType = checkValue(field->defaultValue);

        // nil is allowed for handle types.
        const bool isNil =
            (field->defaultValue->kind == ASTKind::LiteralValue &&
             field->defaultValue->as<LiteralValueAST>()->kind ==
                 LiteralKind::Nil);

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

        // ─── Type-check the node's expression ──────────────────────────────
        if (decl->expr != nullptr)
        {
            checkNodeExpr(decl->expr);
        }

        // ─── The trigger rules ─────────────────────────────────────────────
        //
        // Both rules need the node's NodeTypeInfo. Look it up once.
        const NodeTypeInfo *info = nodeTypeOf(decl);
        if (info != nullptr)
        {
            checkActionHasOn(decl, info);
        }
        checkOnTargets(decl);
    }

    // ─── Trigger rules ────────────────────────────────────────────────────────

    void TypeChecker::checkActionHasOn(const NodeDeclAST *decl,
                                       const NodeTypeInfo *info)
    {
        // Rule 2: an action node must have at least one `on` clause.
        if (info->kind == NodeKind::Action && !decl->hasTriggers())
        {
            m_diag.error(DiagCode::Trigger_ActionWithoutOn, decl,
                         "action node '", name(decl->name),
                         "' has no `on` clause");
        }
    }

    void TypeChecker::checkOnTargets(const NodeDeclAST *decl)
    {
        // Rule 1: every `on` target must resolve to a trigger node.
        //
        // The `on` clause's targets are stored as a span of
        // InternedString in the AST. They do not carry their own AST
        // nodes, so this check looks them up in the symbol table by
        // name. A target that is not in the symbol table is reported
        // by Pass 2 (Name_UndefinedTrigger), not here.
        for (InternedString trigger : decl->triggers)
        {
            const Symbol *symbol = m_symbols.find(trigger);
            if (symbol == nullptr)
            {
                // Pass 2 already reported this.
                continue;
            }
            if (symbol->kind != SymbolKind::Node)
            {
                m_diag.error(DiagCode::Trigger_OnTargetNotTrigger, decl,
                             "`on` target '", name(trigger),
                             "' is not a trigger");
                continue;
            }

            const auto *nodeDecl = symbol->decl->as<NodeDeclAST>();
            const NodeTypeInfo *info = nodeTypeOf(nodeDecl);
            if (info == nullptr)
            {
                // Unknown node type. Pass 3's expression check already
                // reported it.
                continue;
            }
            if (info->kind != NodeKind::Trigger)
            {
                m_diag.error(DiagCode::Trigger_OnTargetNotTrigger, decl,
                             "`on` target '", name(trigger),
                             "' is a ", nodeKindName(info->kind),
                             " node, not a Trigger");
            }
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
        // records the "default" type. Actual argument checking matches
        // the literal's kind against the declared argument type.
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
            const NodeTypeInfo *info = nodeTypeOf(nodeDecl);
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
                // a later pass; for now, accept it and record the enum
                // type by name.
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
        if (node->args.size() != info->args.size())
        {
            m_diag.error(DiagCode::Type_ArgCountMismatch, node,
                         "node type '", nodeTypeName, "' expects ",
                         static_cast<uint64_t>(info->args.size()),
                         " argument(s), but ",
                         static_cast<uint64_t>(node->args.size()),
                         " were given");
            // Continue checking; we still record a type.
        }

        // ─── Check argument types ──────────────────────────────────────────
        const size_t count = node->args.size() < info->args.size()
                                 ? node->args.size()
                                 : info->args.size();

        for (size_t i = 0; i < count; ++i)
        {
            const TypeId argType = checkValue(node->args[i]);
            const TypeId declaredType = info->args[i].type;

            // nil is allowed for handle types.
            const bool isNil =
                (node->args[i] != nullptr &&
                 node->args[i]->kind == ASTKind::LiteralValue &&
                 node->args[i]->as<LiteralValueAST>()->kind ==
                     LiteralKind::Nil);

            if (isNil)
            {
                if (!declaredType.isHandle())
                {
                    m_diag.error(DiagCode::Type_Mismatch, node->args[i],
                                 "nil is only valid for handle types");
                }
                continue;
            }

            if (!argType.isValid())
            {
                // The argument has no value type (a resource, an
                // action node, a trigger node, etc.).
                m_diag.error(DiagCode::Type_InvalidNodeArg, node->args[i],
                             "argument is not a valid value");
                continue;
            }

            if (argType != declaredType)
            {
                m_diag.error(DiagCode::Type_Mismatch, node->args[i],
                             "argument type does not match the declared type");
            }
        }

        // ─── Record the node's result ──────────────────────────────────────
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