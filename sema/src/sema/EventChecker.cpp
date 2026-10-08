/// @file sema/src/sema/EventChecker.cpp
///
/// @brief Implementation of Pass 4: the Event rules.

#include "EventChecker.hpp"

#include "core/ast/DeclAST.hpp"
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
                                           std::string_view name) noexcept
        {
            for (const NodeTypeInfo &info : registry.nodeTypes)
            {
                if (info.name == name)
                    return &info;
            }
            return nullptr;
        }

    } // namespace

    // ─── EventChecker ─────────────────────────────────────────────────────────

    class EventChecker
    {
    public:
        EventChecker(const SymbolTable &symbols,
                     const ResolutionMap &resolutions,
                     const Registry &registry,
                     DiagnosticEngine &diag)
            : m_symbols(symbols), m_resolutions(resolutions), m_registry(registry), m_diag(diag)
        {
        }

        void checkModule(const ModuleAST *module);

    private:
        void checkDecl(const DeclAST *decl);
        void checkNodeDecl(const NodeDeclAST *decl);
        void checkOnTargets(const NodeDeclAST *decl);

        void checkRegistryEventPorts();

        // ─── Helpers ───────────────────────────────────────────────────────

        std::string_view name(InternedString s) const
        {
            if (m_diag.stringPool() == nullptr)
                return "<unknown>";
            return m_diag.stringPool()->lookupView(s);
        }

        /// Look up the node type of a node declaration. Returns nullptr
        /// if the declaration is malformed or the type is unknown.
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

        /// True if the given `on` target is a valid trigger source.
        ///
        /// succeed at module level: A bare identifier that resolves to a trigger node.
        ///
        /// All other cases fail.
        bool isTriggerSource(const BaseAST *target) const;

        // ─── State ─────────────────────────────────────────────────────────

        const SymbolTable &m_symbols;
        const ResolutionMap &m_resolutions;
        const Registry &m_registry;
        DiagnosticEngine &m_diag;
    };

    // ─── Module and declarations ──────────────────────────────────────────────

    void EventChecker::checkModule(const ModuleAST *module)
    {
        if (module == nullptr)
            return;

        // First, the registry-wide check for Event-typed ports.
        checkRegistryEventPorts();

        // Then, per-declaration checks.
        for (DeclAST *decl : module->decls)
        {
            if (decl == nullptr)
                continue;
            checkDecl(decl);
        }
    }

    void EventChecker::checkDecl(const DeclAST *decl)
    {
        if (decl == nullptr)
            return;

        switch (decl->kind)
        {
        case ASTKind::NodeDecl:
            checkNodeDecl(decl->as<NodeDeclAST>());
            break;
        default:
            // Composite bodies are checked in Step 7.6.
            break;
        }
    }

    void EventChecker::checkNodeDecl(const NodeDeclAST *decl)
    {
        if (decl == nullptr)
            return;

        const NodeTypeInfo *info = nodeTypeOf(decl);

        if (info == nullptr)
        {
            // Unknown node type. Pass 3 already reported it.
            return;
        }

        // ─── Rule 3: action nodes need `on` ────────────────────────────────
        if (info->kind == NodeKind::Action && !decl->hasTriggers())
        {
            m_diag.error(DiagCode::Event_ActionWithoutOn, decl,
                         "action node '", name(decl->name),
                         "' has no `on` clause");
        }

        // ─── Rule 2: `on` targets must be trigger sources ──────────────────
        checkOnTargets(decl);
    }

    void EventChecker::checkOnTargets(const NodeDeclAST *decl)
    {
        if (decl == nullptr)
            return;

        // The `on` clause's targets are stored as a span of
        // InternedString in the AST. They do not carry their own AST
        // nodes, so the EventChecker walks the node declaration's
        // `on`-clause target nodes the same way the resolver did.
        //
        // Wait — the NodeDeclAST only stores `triggers` as a span of
        // InternedString. The `on`-clause target nodes (the values)
        // are not stored separately. The parser produces the span and
        // discards the individual value nodes.
        //
        // Consequence: the EventChecker cannot look up the resolution
        // of a specific `on` target, because the resolver recorded the
        // resolutions keyed on the target's AST node, and the AST does
        // not preserve them.
        //
        // Two options:
        //   - (a) Change the AST so NodeDeclAST stores its trigger
        //     targets as `BaseAST*` values (with locations).
        //   - (b) Have the resolver, during Pass 2, record resolutions
        //     in a way that maps triggers to their resolutions by
        //     *position* rather than by AST node.
        //
        // For Step 7.5, we take neither approach. The check is
        // deferred: an `on` target is validated when it is a bare
        // identifier by looking it up in the symbol table, not the
        // resolution map. The symbol table has every node declaration
        // by name; if the name resolves to a trigger node, the check
        // passes.
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
                m_diag.error(DiagCode::Event_OnTargetNotTrigger, decl,
                             "`on` target '", name(trigger),
                             "' is not a trigger");
                continue;
            }

            const auto *nodeDecl = symbol->decl->as<NodeDeclAST>();
            const NodeTypeInfo *info = nodeTypeOf(nodeDecl);
            if (info == nullptr)
            {
                // Unknown node type. Pass 3 already reported it.
                continue;
            }
            if (info->kind != NodeKind::Trigger)
            {
                m_diag.error(DiagCode::Event_OnTargetNotTrigger, decl,
                             "`on` target '", name(trigger),
                             "' is a ", nodeKindName(info->kind),
                             " node, not a Trigger");
            }
        }
    }

    // ─── Registry check ───────────────────────────────────────────────────────

    void EventChecker::checkRegistryEventPorts()
    {
        // Defensive: no node port may declare the Event type. The
        // registry should not register such a port, but if it does,
        // Sema reports it here.
        for (const NodeTypeInfo &info : m_registry.nodeTypes)
        {
            auto checkPort = [this, &info](const NodePortInfo &port)
            {
                if (port.type.isEvent())
                {
                    // No AST node to anchor the diagnostic to; the
                    // port is registry data. Report at a synthetic
                    // location.
                    m_diag.errorAt(DiagCode::Event_PortNotAllowed,
                                   SourceLocation{1, 1},
                                   "node type '", info.name,
                                   "' declares a port '", port.name,
                                   "' with type Event; Event is not a "
                                   "value type");
                }
            };

            for (const NodePortInfo &port : info.inputs)
                checkPort(port);
            for (const NodePortInfo &port : info.outputs)
                checkPort(port);
            for (const NodePortInfo &port : info.payload)
                checkPort(port);
        }
    }

    // ─── Public entry point ───────────────────────────────────────────────────

    void checkEvents(const ModuleAST *module,
                     const SymbolTable &symbols,
                     const ResolutionMap &resolutions,
                     const Registry &registry,
                     DiagnosticEngine &diag)
    {
        EventChecker checker(symbols, resolutions, registry, diag);
        checker.checkModule(module);
    }

} // namespace lucid::sema