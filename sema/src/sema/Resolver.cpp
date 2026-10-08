/// @file sema/src/sema/Resolver.cpp
///
/// @brief Implementation of Pass 2: name resolution.
///
/// ─── The walk ─────────────────────────────────────────────────────────────
/// One function per AST kind. Each function resolves the names it
/// contains and recurses into the sub-nodes that can contain more
/// references. The walk covers every declaration's body, every value,
/// every type, and every node expression.
///
/// ─── Scope ────────────────────────────────────────────────────────────────
/// In the current grammar, the only scopes are the module scope and a
/// composite's local scope. A composite's body can reference the
/// composite's inputs, its resources, its nodes, and the enclosing
/// module's symbols. It cannot reference another composite's internals.
///
/// For Step 7.3, the resolver treats the module scope as the only
/// scope. Composite bodies are resolved against the module's symbol
/// table, plus a local scope built from the composite's inputs and
/// internal declarations. Composite-local resolution is a Step 7.6
/// concern (when composites are expanded); for now, the resolver
/// resolves composite-internal names against the composite itself as
/// best it can, and defers the rest.

#include "Resolver.hpp"

#include "core/ast/AttributeAST.hpp"
#include "core/ast/DeclAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/ast/ValueAST.hpp"

namespace lucid::sema
{

    namespace
    {

        // ─── Helper: resolve a single name ─────────────────────────────────

        /// Look up `name` in the local symbol table. If found, record
        /// the resolution and return the target. Otherwise, record a
        /// deferred (null) resolution and return null.
        ///
        /// The caller decides whether to report an error for a null
        /// result. Some references are deferred without an error (import
        /// aliases); others are errors (unknown local names).
        const BaseAST *resolveLocal(const SymbolTable &symbols,
                                    ResolutionMap &resolutions,
                                    const BaseAST *ref,
                                    InternedString name)
        {
            const Symbol *symbol = symbols.find(name);
            if (symbol != nullptr)
            {
                resolutions.record(ref, symbol->decl);
                return symbol->decl;
            }
            resolutions.record(ref, nullptr);
            return nullptr;
        }

    } // namespace

    // ─── Value resolution ─────────────────────────────────────────────────────

    class Resolver
    {
    public:
        Resolver(const SymbolTable &symbols,
                 ResolutionMap &resolutions,
                 lucid::diag::DiagnosticEngine &diag)
            : m_symbols(symbols), m_resolutions(resolutions), m_diag(diag)
        {
        }

        void resolveModule(const ModuleAST *module);

    private:
        // ─── Declarations ──────────────────────────────────────────────────

        void resolveDecl(const DeclAST *decl);
        void resolveImportDecl(const ImportDeclAST *decl);
        void resolveEnumDecl(const EnumDeclAST *decl);
        void resolveResourceDecl(const ResourceDeclAST *decl);
        void resolveNodeDecl(const NodeDeclAST *decl);
        void resolveCompositeDecl(const CompositeDeclAST *decl);

        // ─── Fields, inputs, outputs ───────────────────────────────────────

        void resolveResourceField(const ResourceFieldAST *field);
        void resolveCompositeInput(const CompositeInputAST *input);
        void resolveCompositeOutput(const CompositeOutputAST *output);

        // ─── Values ────────────────────────────────────────────────────────

        void resolveValue(const BaseAST *value);
        void resolveLiteralValue(const LiteralValueAST *value);
        void resolveIdentifierValue(const IdentifierValueAST *value);
        void resolveFieldAccessValue(const FieldAccessValueAST *value);
        void resolveInlineNodeValue(const InlineNodeValueAST *value);
        void resolveNodeExpr(const NodeExprAST *node);

        // ─── Types ─────────────────────────────────────────────────────────

        void resolveTypeId(const TypeIdAST *type);

        // ─── Triggers ──────────────────────────────────────────────────────

        void resolveTriggers(ArenaSpan<InternedString> triggers,
                             const NodeDeclAST *nodeDecl);

        // ─── State ─────────────────────────────────────────────────────────

        const SymbolTable &m_symbols;
        ResolutionMap &m_resolutions;
        lucid::diag::DiagnosticEngine &m_diag;
    };

    // ─── Module ───────────────────────────────────────────────────────────────

    void Resolver::resolveModule(const ModuleAST *module)
    {
        if (module == nullptr)
            return;

        for (DeclAST *decl : module->decls)
        {
            if (decl == nullptr)
                continue;
            resolveDecl(decl);
        }
    }

    // ─── Declarations ─────────────────────────────────────────────────────────

    void Resolver::resolveDecl(const DeclAST *decl)
    {
        if (decl == nullptr)
            return;

        switch (decl->kind)
        {
        case ASTKind::ImportDecl:
            resolveImportDecl(decl->as<ImportDeclAST>());
            break;
        case ASTKind::EnumDecl:
            resolveEnumDecl(decl->as<EnumDeclAST>());
            break;
        case ASTKind::ResourceDecl:
            resolveResourceDecl(decl->as<ResourceDeclAST>());
            break;
        case ASTKind::NodeDecl:
            resolveNodeDecl(decl->as<NodeDeclAST>());
            break;
        case ASTKind::CompositeDecl:
            resolveCompositeDecl(decl->as<CompositeDeclAST>());
            break;
        default:
            break;
        }
    }

    void Resolver::resolveImportDecl(const ImportDeclAST *decl)
    {
        if (decl == nullptr)
            return;

        // The import's path is resolved by the import resolver, not
        // here. The alias is a symbol that Pass 1 already added. There
        // is nothing to resolve in this declaration.
        (void)decl;
    }

    void Resolver::resolveEnumDecl(const EnumDeclAST *decl)
    {
        if (decl == nullptr)
            return;

        // Enum members are names; they have no references to resolve.
        // The enum itself is a symbol; the script's enum declaration is
        // validated against the registry in Step 7.4.
        (void)decl;
    }

    void Resolver::resolveResourceDecl(const ResourceDeclAST *decl)
    {
        if (decl == nullptr)
            return;

        for (ResourceFieldAST *field : decl->fields)
        {
            resolveResourceField(field);
        }
    }

    void Resolver::resolveResourceField(const ResourceFieldAST *field)
    {
        if (field == nullptr)
            return;

        if (field->type != nullptr)
        {
            resolveTypeId(field->type);
        }

        if (field->defaultValue != nullptr)
        {
            resolveLiteralValue(field->defaultValue);
        }
    }

    void Resolver::resolveNodeDecl(const NodeDeclAST *decl)
    {
        if (decl == nullptr)
            return;

        // The node's expression.
        if (decl->expr != nullptr)
        {
            resolveNodeExpr(decl->expr);
        }

        // The node's triggers. Each trigger name is a reference to
        // another node in the same module (or to a composite output
        // exposed as an Event, which is deferred to Step 7.6).
        resolveTriggers(decl->triggers, decl);
    }

    void Resolver::resolveTriggers(ArenaSpan<InternedString> triggers,
                                   const NodeDeclAST *nodeDecl)
    {
        for (InternedString triggerName : triggers)
        {
            const Symbol *symbol = m_symbols.find(triggerName);

            if (symbol == nullptr)
            {
                // Report the unresolved trigger. The location points
                // at the node declaration, since the trigger span's
                // elements do not carry their own locations.
                //
                // A limitation: all unresolved triggers on the same
                // node report at the node's location, not at each
                // trigger's position. If per-trigger locations are
                // needed later, the AST's trigger span must change to
                // a span of nodes (like EnumMemberAST).
                m_diag.error(diag::DiagCode::Name_UndefinedTrigger,
                             nodeDecl,
                             "undefined trigger '",
                             m_diag.stringPool()
                                 ? m_diag.stringPool()->lookupView(triggerName)
                                 : std::string_view{"<unknown>"},
                             "'");
                continue;
            }

            // Record the resolution. The trigger's "reference node" is
            // the enclosing node declaration; Step 7.6 walks the
            // resolution map to find which nodes subscribe to which
            // triggers.
            m_resolutions.record(nodeDecl, symbol->decl);
        }
    }

    void Resolver::resolveCompositeDecl(const CompositeDeclAST *decl)
    {
        if (decl == nullptr)
            return;

        // Composite inputs and outputs are the composite's interface.
        // They can reference the enclosing module's symbols (e.g., a
        // type in `input { max: int }`), and their resolution is part
        // of the module's resolution.
        //
        // The composite's body — its internal resources, enums, and
        // nodes — is resolved in Step 7.6 when the composite is
        // expanded. The body's references are to the composite's local
        // scope, which is not available here.
        for (CompositeInputAST *input : decl->inputs)
        {
            resolveCompositeInput(input);
        }

        for (CompositeOutputAST *output : decl->outputs)
        {
            resolveCompositeOutput(output);
        }

        // Intentionally do not descend into decl->body. Step 7.6 does.
        (void)decl->body;
    }

    void Resolver::resolveCompositeInput(const CompositeInputAST *input)
    {
        if (input == nullptr)
            return;

        if (input->type != nullptr)
        {
            resolveTypeId(input->type);
        }
    }

    void Resolver::resolveCompositeOutput(const CompositeOutputAST *output)
    {
        if (output == nullptr)
            return;

        if (output->type != nullptr)
        {
            resolveTypeId(output->type);
        }

        if (output->value != nullptr)
        {
            resolveValue(output->value);
        }
    }

    // ─── Values ───────────────────────────────────────────────────────────────

    void Resolver::resolveValue(const BaseAST *value)
    {
        if (value == nullptr)
            return;

        switch (value->kind)
        {
        case ASTKind::LiteralValue:
            resolveLiteralValue(value->as<LiteralValueAST>());
            break;
        case ASTKind::IdentifierValue:
            resolveIdentifierValue(value->as<IdentifierValueAST>());
            break;
        case ASTKind::FieldAccessValue:
            resolveFieldAccessValue(value->as<FieldAccessValueAST>());
            break;
        case ASTKind::InlineNodeValue:
            resolveInlineNodeValue(value->as<InlineNodeValueAST>());
            break;
        default:
            break;
        }
    }

    void Resolver::resolveLiteralValue(const LiteralValueAST *value)
    {
        // Literals contain no names. Nothing to resolve.
        (void)value;
    }

    void Resolver::resolveIdentifierValue(const IdentifierValueAST *value)
    {
        if (value == nullptr)
            return;

        const BaseAST *target =
            resolveLocal(m_symbols, m_resolutions, value, value->name);

        if (target == nullptr)
        {
            m_diag.error(diag::DiagCode::Name_UndefinedNode,
                         value,
                         "undefined name '",
                         m_diag.stringPool()
                             ? m_diag.stringPool()->lookupView(value->name)
                             : std::string_view{"<unknown>"},
                         "'");
        }
    }

    void Resolver::resolveFieldAccessValue(const FieldAccessValueAST *value)
    {
        if (value == nullptr)
            return;

        // The object might be:
        //   - A resource: `Config.speed` → resolve `Config`, then look
        //     up `speed` among its fields.
        //   - An enum: `Key.W` → resolve `Key`, then look up `W` in
        //     the registry's enum members (Step 7.4).
        //   - A composite output: `player_health.current` → resolve
        //     `player_health` (a composite use), then look up `current`
        //     among the composite's outputs (Step 7.6).
        //   - An import alias: `health.Health` → resolve `health` (an
        //     import), then look up `Health` in the imported module's
        //     symbol table (Step 7.8).
        //
        // For Step 7.3, we resolve the object and record a deferred
        // resolution for the field. The specific field lookup happens
        // in later steps when the object's kind is known.
        const Symbol *objectSymbol = m_symbols.find(value->object);

        if (objectSymbol == nullptr)
        {
            m_diag.error(diag::DiagCode::Name_UndefinedNode,
                         value,
                         "undefined name '",
                         m_diag.stringPool()
                             ? m_diag.stringPool()->lookupView(value->object)
                             : std::string_view{"<unknown>"},
                         "'");
            m_resolutions.record(value, nullptr);
            return;
        }

        // Record the object's resolution. The full field resolution
        // is deferred: the resolver records the object's symbol as
        // the "resolved object," and Step 7.4/7.6/7.8 resolve the
        // field against the object's kind.
        m_resolutions.record(value, objectSymbol->decl);
    }

    void Resolver::resolveInlineNodeValue(const InlineNodeValueAST *value)
    {
        if (value == nullptr)
            return;
        if (value->node != nullptr)
        {
            resolveNodeExpr(value->node);
        }
    }

    void Resolver::resolveNodeExpr(const NodeExprAST *node)
    {
        if (node == nullptr)
            return;

        if (node->type != nullptr)
        {
            resolveTypeId(node->type);
        }

        for (BaseAST *arg : node->args)
        {
            resolveValue(arg);
        }
    }

    // ─── Types ────────────────────────────────────────────────────────────────

    void Resolver::resolveTypeId(const TypeIdAST *type)
    {
        if (type == nullptr)
            return;

        // A type name might be:
        //   - A primitive: `float32`, `int32`, etc.
        //   - A handle: `BodyRef`, `TextureRef`, etc.
        //   - An enum: `Key`, `Direction`.
        //   - A resource: a resource used as a type in a field.
        //
        // Primitives and handles are registry concerns; Step 7.4 checks
        // them against the registry. Enums and resources are module
        // symbols; the resolver records their resolution.
        //
        // For Step 7.3, we record the resolution if the name is in the
        // symbol table. If not, we defer: the name might be a registry
        // type checked in Step 7.4.
        const Symbol *symbol = m_symbols.find(type->name);

        if (symbol != nullptr &&
            (symbol->kind == SymbolKind::Enum ||
             symbol->kind == SymbolKind::Resource))
        {
            m_resolutions.record(type, symbol->decl);
        }
        else
        {
            // Not a module symbol. Might be a primitive or handle
            // (Step 7.4), or a qualified type through an import
            // (Step 7.8). Defer.
            m_resolutions.record(type, nullptr);
        }
    }

    // ─── Public entry point ───────────────────────────────────────────────────

    void resolveNames(const ModuleAST *module,
                      const SymbolTable &symbols,
                      ResolutionMap &resolutions,
                      lucid::diag::DiagnosticEngine &diag)
    {
        Resolver resolver(symbols, resolutions, diag);
        resolver.resolveModule(module);
    }

} // namespace lucid::sema
