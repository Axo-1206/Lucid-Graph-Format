/// @file sema/src/sema/GraphBuilder.cpp
///
/// @brief Implementation of Pass 4: graph construction.

#include "GraphBuilder.hpp"

#include "core/ast/DeclAST.hpp"
#include "core/ast/ValueAST.hpp"
#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/StringPool.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

using namespace lucid::diag;

namespace lucid::sema
{

    namespace
    {

        /// Find a node type's index in the registry by name. Returns
        /// UINT32_MAX if not found.
        uint32_t nodeTypeIndex(const Registry &registry,
                               std::string_view name) noexcept
        {
            for (size_t i = 0; i < registry.nodeTypes.size(); ++i)
            {
                if (registry.nodeTypes[i].name == name)
                {
                    return static_cast<uint32_t>(i);
                }
            }
            return UINT32_MAX;
        }

        /// Resolve a resource field's written default to a Literal.
        /// Returns a nil Literal if the value cannot be resolved; Sema
        /// is expected to have reported the error already.
        Literal resolveFieldValue(const BaseAST *value,
                                  const ConstantValueMap *constants,
                                  const DiagnosticEngine &diag,
                                  std::vector<char> &string_pool)
        {
            if (value == nullptr)
                return Literal{};

            // String literals need the graph's string pool; the offset
            // is computed here.
            if (value->kind == ASTKind::LiteralValue)
            {
                const auto *lit = value->as<LiteralValueAST>();
                if (lit->kind == LiteralKind::String)
                {
                    const std::string_view s =
                        diag.stringPool()
                            ? diag.stringPool()->lookupView(lit->text)
                            : std::string_view{};
                    const uint32_t offset =
                        static_cast<uint32_t>(string_pool.size());
                    string_pool.insert(string_pool.end(),
                                       s.begin(), s.end());
                    return Literal::makeString(
                        offset, static_cast<uint32_t>(s.size()));
                }
            }

            // Non-string values are looked up in the constant map.
            if (constants != nullptr)
            {
                const Literal *folded = constants->lookup(value);
                if (folded != nullptr)
                    return *folded;
            }

            return Literal{};
        }

    } // namespace

    // ─── GraphBuilder ─────────────────────────────────────────────────────────

    class GraphBuilder
    {
    public:
        GraphBuilder(const Registry &registry,
                     StringPool &pool,
                     DiagnosticEngine &diag)
            : m_registry(registry), m_pool(pool), m_diag(diag)
        {
        }

        std::unique_ptr<Graph> build(const std::vector<ModuleContext> &modules);

    private:
        // ─── Phase A: registration ─────────────────────────────────────────
        void registerDecls(const ModuleAST *module);

        // ─── Phase B: building ─────────────────────────────────────────────
        void buildDecls(const ModuleAST *module);

        NodeInstance buildNode(const NodeDeclAST *decl);
        Resource buildResource(const ResourceDeclAST *decl);
        ResourceField buildResourceField(const ResourceFieldAST *field);

        Arg buildArg(const BaseAST *value);

        // ─── Phase C: order and subscribers ────────────────────────────────
        void buildSubscribers(const ModuleAST *module);
        void buildPhaseOrder();
        void buildValueOrder();

        // ─── Struct ───────────────────────────────────────────────────────

        Literal zeroStructValue(const TypeId &type);
        void appendZeroFieldBytes(const TypeId &type);
        void appendLiteralWireBytes(const Literal &lit);

        // ─── Helpers ───────────────────────────────────────────────────────
        Literal literalFor(const BaseAST *value) const;
        uint32_t addStringToPool(std::string_view s);

        bool isValueNode(NodeIndex idx) const;
        bool isActionNode(NodeIndex idx) const;

        NodeIndex buildInlineNode(const NodeExprAST *expr);

        // ─── Per-module context (set at the start of each module's phase) ──
        // These are reset before each phase-B and phase-C iteration.
        const SymbolTable *m_symbols = nullptr;
        const ResolutionMap *m_resolutions = nullptr;
        const ConstantValueMap *m_constants = nullptr;
        const TypeMap *m_types = nullptr;

        // ─── Shared state ──────────────────────────────────────────────────
        const Registry &m_registry;
        StringPool &m_pool;
        DiagnosticEngine &m_diag;

        Graph m_graph;

        std::unordered_map<const NodeDeclAST *, NodeIndex> m_nodeIndex;
        std::unordered_map<const ResourceDeclAST *, uint32_t> m_resourceIndex;
        std::unordered_map<const ResourceFieldAST *, uint32_t> m_fieldIndex;

        // A resource field's index is global across all resources; it
        // indexes into Graph::resource_fields. The (resourceIndex,
        // fieldIndex) pair identifies a field for an Arg::ResourceRef.
        // The map above stores the *global* field index.

        // Per-node lists of subscribers, collected before flattening.
        std::vector<std::vector<NodeIndex>> m_subscribersByNode;
    };

    // ─── build ────────────────────────────────────────────────────────────────

    std::unique_ptr<Graph> GraphBuilder::build(
        const std::vector<ModuleContext> &modules)
    {
        // ─── Phase A: register every module's nodes and resources ──────────
        for (const ModuleContext &ctx : modules)
        {
            registerDecls(ctx.module);
        }

        // ─── Phase B: build every module's nodes and resources ─────────────
        for (const ModuleContext &ctx : modules)
        {
            m_symbols = ctx.symbols;
            m_resolutions = ctx.resolutions;
            m_constants = ctx.constants;
            m_types = ctx.types;
            buildDecls(ctx.module);
        }

        // ─── Phase C: subscribers ──────────────────────────────────────────
        for (const ModuleContext &ctx : modules)
        {
            m_symbols = ctx.symbols;
            m_resolutions = ctx.resolutions;
            m_constants = ctx.constants;
            m_types = ctx.types;
            buildSubscribers(ctx.module);
        }

        // phase_order and value_order are global; no module context needed.
        buildPhaseOrder();
        buildValueOrder();

        // The fingerprint identifies the registry this graph was
        // compiled against. serialize() writes it into the .lucgraph
        // header; deserialize() checks it against the loader's
        // registry and refuses a mismatch.
        m_graph.registry_fingerprint = computeRegistryFingerprint(m_registry);

        // The string pool now holds every literal the graph uses, and
        // nothing else: `addStringToPool` is the only writer. The
        // literal prefix is therefore the whole pool. `serialize`
        // starts from this prefix and appends names; see
        // Graph::literal_pool_size.
        m_graph.literal_pool_size = static_cast<uint32_t>(m_graph.string_pool.size());

        return std::make_unique<Graph>(std::move(m_graph));
    }

    // ─── Phase A: registration ────────────────────────────────────────────────

    void GraphBuilder::registerDecls(const ModuleAST *module)
    {
        if (module == nullptr)
            return;

        for (DeclAST *decl : module->decls)
        {
            if (decl == nullptr)
                continue;

            if (decl->kind == ASTKind::NodeDecl)
            {
                auto *nodeDecl = decl->as<NodeDeclAST>();
                const NodeIndex idx =
                    static_cast<NodeIndex>(m_graph.nodes.size());
                m_graph.nodes.emplace_back(); // placeholder
                m_nodeIndex[nodeDecl] = idx;
                continue;
            }

            if (decl->kind == ASTKind::ResourceDecl)
            {
                auto *resDecl = decl->as<ResourceDeclAST>();
                const uint32_t resIdx =
                    static_cast<uint32_t>(m_resourceIndex.size());
                m_resourceIndex[resDecl] = resIdx;

                for (ResourceFieldAST *field : resDecl->fields)
                {
                    if (field != nullptr)
                    {
                        const uint32_t fieldIdx =
                            static_cast<uint32_t>(m_fieldIndex.size());
                        m_fieldIndex[field] = fieldIdx;
                    }
                }
                continue;
            }

            // Imports and enums are not represented in the graph.
        }

        m_subscribersByNode.resize(m_graph.nodes.size());
    }

    // ─── Phase B: building ────────────────────────────────────────────────────

    void GraphBuilder::buildDecls(const ModuleAST *module)
    {
        for (DeclAST *decl : module->decls)
        {
            if (decl == nullptr)
                continue;

            if (decl->kind == ASTKind::NodeDecl)
            {
                auto *nodeDecl = decl->as<NodeDeclAST>();
                const NodeIndex idx = m_nodeIndex[nodeDecl];
                m_graph.nodes[idx] = buildNode(nodeDecl);
                continue;
            }

            if (decl->kind == ASTKind::ResourceDecl)
            {
                auto *resDecl = decl->as<ResourceDeclAST>();
                m_graph.resources.push_back(buildResource(resDecl));

                for (ResourceFieldAST *field : resDecl->fields)
                {
                    if (field != nullptr)
                    {
                        m_graph.resource_fields.push_back(
                            buildResourceField(field));
                    }
                }
                continue;
            }
        }
    }

    NodeInstance GraphBuilder::buildNode(const NodeDeclAST *decl)
    {
        NodeInstance inst;

        // ─── type_id ───────────────────────────────────────────────────────
        if (decl->expr != nullptr && decl->expr->type != nullptr)
        {
            const std::string_view typeName =
                m_diag.stringPool()
                    ? m_diag.stringPool()->lookupView(decl->expr->type->name)
                    : std::string_view{};
            inst.type_id = nodeTypeIndex(m_registry, typeName);

            // Look up the phase.
            const NodeTypeInfo *info = nullptr;
            for (const auto &ni : m_registry.nodeTypes)
            {
                if (ni.name == typeName)
                {
                    info = &ni;
                    break;
                }
            }
            if (info != nullptr)
            {
                inst.phase = info->phase;
            }
        }

        // ─── args ──────────────────────────────────────────────────────────
        //
        // Build the node's arguments into a temporary buffer, then
        // append them contiguously. Building an argument may
        // recursively append args to m_graph.args (for inline nodes);
        // collecting into a buffer first keeps the node's own args
        // contiguous and correctly offset.
        if (decl->expr != nullptr)
        {
            std::vector<Arg> tempArgs;
            tempArgs.reserve(decl->expr->args.size());
            for (BaseAST *arg : decl->expr->args)
            {
                tempArgs.push_back(buildArg(arg));
            }

            inst.args_offset = static_cast<uint32_t>(m_graph.args.size());
            for (Arg &a : tempArgs)
            {
                m_graph.args.push_back(std::move(a));
            }
            inst.args_count = static_cast<uint32_t>(tempArgs.size());
        }

        // subscribers_offset/count are set in buildSubscribers.
        return inst;
    }

    Resource GraphBuilder::buildResource(const ResourceDeclAST *decl)
    {
        Resource res;
        if (m_diag.stringPool() != nullptr)
        {
            const std::string_view view =
                m_diag.stringPool()->lookupView(decl->name);
            res.name.assign(view.data(), view.size());
        }
        res.fields_offset = static_cast<uint32_t>(m_graph.resource_fields.size());
        res.fields_count = static_cast<uint32_t>(decl->fields.size());
        return res;
    }

    ResourceField GraphBuilder::buildResourceField(const ResourceFieldAST *field)
    {
        ResourceField rf;
        if (m_diag.stringPool() != nullptr)
        {
            const std::string_view view =
                m_diag.stringPool()->lookupView(field->name);
            rf.name.assign(view.data(), view.size());
        }

        if (field->type != nullptr)
        {
            rf.type = m_types->lookup(field->type);
        }

        // ─── The field's value ─────────────────────────────────────────────
        //
        // A resource field's value is a compile-time constant. The
        // source may write `= value`; if it does not, the value is the
        // type's zero. In both cases the graph stores a resolved
        // `Literal`. The runtime never sees "no value" — every field
        // has a value.
        if (field->defaultValue != nullptr)
        {
            // The source wrote a value. Resolve it to a Literal.
            rf.value = resolveFieldValue(field->defaultValue, m_constants,
                                         m_diag, m_graph.string_pool);
        }
        else
        {
            if (rf.type.kind == TypeId::Kind::Struct)
                rf.value = zeroStructValue(rf.type);
            else
                rf.value = zeroLiteralOf(rf.type, m_registry);
        }

        return rf;
    }

    // ─── Argument construction ────────────────────────────────────────────────

    Arg GraphBuilder::buildArg(const BaseAST *value)
    {
        if (value == nullptr)
        {
            return Arg::makeLiteral(Literal{});
        }

        switch (value->kind)
        {
        // ─── Literal ───────────────────────────────────────────────────────
        case ASTKind::LiteralValue:
        {
            const auto *lit = value->as<LiteralValueAST>();

            // Strings are handled specially: their bytes live in the
            // pool, and the (offset, length) is computed here.
            if (lit->kind == LiteralKind::String)
            {
                const std::string_view s =
                    m_diag.stringPool()
                        ? m_diag.stringPool()->lookupView(lit->text)
                        : std::string_view{};
                const uint32_t offset = addStringToPool(s);
                return Arg::makeLiteral(
                    Literal::makeString(offset,
                                        static_cast<uint32_t>(s.size())));
            }

            // Other literals are recorded in the constant map.
            const Literal *folded = m_constants->lookup(value);
            if (folded != nullptr)
            {
                return Arg::makeLiteral(*folded);
            }
            return Arg::makeLiteral(Literal{});
        }

        // ─── Identifier (a reference to another node) ──────────────────────
        case ASTKind::IdentifierValue:
        {
            const BaseAST *target = m_resolutions->lookup(value);
            if (target != nullptr && target->kind == ASTKind::NodeDecl)
            {
                const auto *targetDecl = target->as<NodeDeclAST>();
                auto it = m_nodeIndex.find(targetDecl);
                if (it != m_nodeIndex.end())
                {
                    return Arg::makeNodeRef(it->second);
                }
            }
            // Reference to a resource by bare name, or an unresolved
            // name. The type checker should have caught this. Emit a
            // placeholder Nil.
            return Arg::makeLiteral(Literal{});
        }

        // ─── Field access (a reference to a resource field, or an enum member) ──
        case ASTKind::FieldAccessValue:
        {
            const BaseAST *target = m_resolutions->lookup(value);
            if (target != nullptr && target->kind == ASTKind::ResourceDecl)
            {
                const auto *resDecl = target->as<ResourceDeclAST>();
                auto resIt = m_resourceIndex.find(resDecl);
                if (resIt == m_resourceIndex.end())
                {
                    return Arg::makeLiteral(Literal{});
                }

                // Find the field.
                const auto *fieldAccess = value->as<FieldAccessValueAST>();
                for (ResourceFieldAST *field : resDecl->fields)
                {
                    if (field != nullptr && field->name == fieldAccess->field)
                    {
                        auto fieldIt = m_fieldIndex.find(field);
                        if (fieldIt != m_fieldIndex.end())
                        {
                            return Arg::makeResourceRef(resIt->second,
                                                        fieldIt->second);
                        }
                        break;
                    }
                }
                return Arg::makeLiteral(Literal{});
            }

            if (target != nullptr && target->kind == ASTKind::EnumDecl)
            {
                // An enum member reference. Its value is in the
                // constant map.
                const Literal *folded = m_constants->lookup(value);
                if (folded != nullptr)
                {
                    return Arg::makeLiteral(*folded);
                }
                return Arg::makeLiteral(Literal{});
            }

            // Something else (not a resource field or enum member).
            // The type checker should have caught this.
            return Arg::makeLiteral(Literal{});
        }

        // ─── Inline node ───────────────────────────────────────────────────
        case ASTKind::InlineNodeValue:
        {
            const auto *inlineNode = value->as<InlineNodeValueAST>();
            if (inlineNode->node == nullptr)
            {
                return Arg::makeLiteral(Literal{});
            }

            const NodeIndex idx = buildInlineNode(inlineNode->node);
            return Arg::makeNodeRef(idx);
        }

        default:
            return Arg::makeLiteral(Literal{});
        }
    }

    // ─── literalFor ───────────────────────────────────────────────────────────

    Literal GraphBuilder::literalFor(const BaseAST *value) const
    {
        if (value == nullptr)
        {
            return Literal{};
        }

        if (value->kind == ASTKind::LiteralValue)
        {
            const auto *lit = value->as<LiteralValueAST>();
            if (lit->kind == LiteralKind::String)
            {
                // String literals are not stored in the constant map.
                // Return a nil; the caller is expected to handle
                // strings specially.
                return Literal{};
            }
        }

        const Literal *folded = m_constants->lookup(value);
        if (folded != nullptr)
        {
            return *folded;
        }
        return Literal{};
    }

    // ─── addStringToPool ──────────────────────────────────────────────────────

    uint32_t GraphBuilder::addStringToPool(std::string_view s)
    {
        const uint32_t offset =
            static_cast<uint32_t>(m_graph.string_pool.size());
        m_graph.string_pool.insert(m_graph.string_pool.end(),
                                   s.begin(), s.end());
        return offset;
    }

    // ─── Phase C: subscribers ─────────────────────────────────────────────────

    void GraphBuilder::buildSubscribers(const ModuleAST *module)
    {
        m_subscribersByNode.resize(m_graph.nodes.size());

        // Collect subscribers per target node.
        for (DeclAST *decl : module->decls)
        {
            if (decl == nullptr || decl->kind != ASTKind::NodeDecl)
            {
                continue;
            }

            const auto *nodeDecl = decl->as<NodeDeclAST>();
            const NodeIndex subscriberIdx = m_nodeIndex[nodeDecl];

            for (InternedString triggerName : nodeDecl->triggers)
            {
                const Symbol *sym = m_symbols->find(triggerName);
                if (sym == nullptr || sym->kind != SymbolKind::Node)
                {
                    continue;
                }

                const auto *targetDecl = sym->decl->as<NodeDeclAST>();
                auto it = m_nodeIndex.find(targetDecl);
                if (it == m_nodeIndex.end())
                {
                    continue;
                }

                m_subscribersByNode[it->second].push_back(subscriberIdx);
            }
        }

        // Flatten into Graph::subscribers.
        for (NodeIndex i = 0; i < m_graph.nodes.size(); ++i)
        {
            auto &list = m_subscribersByNode[i];
            NodeInstance &inst = m_graph.nodes[i];
            inst.subscribers_offset =
                static_cast<uint32_t>(m_graph.subscribers.size());
            inst.subscribers_count = static_cast<uint32_t>(list.size());
            for (NodeIndex sub : list)
            {
                m_graph.subscribers.push_back(sub);
            }
        }
    }

    // ─── Phase C: phase order ─────────────────────────────────────────────────

    void GraphBuilder::buildPhaseOrder()
    {

        // Collect action nodes with their phase.
        struct ActionOrder
        {
            uint32_t phase;
            NodeIndex index;
        };
        std::vector<ActionOrder> actions;

        for (NodeIndex i = 0; i < m_graph.nodes.size(); ++i)
        {
            const NodeInstance &inst = m_graph.nodes[i];
            if (inst.type_id == UINT32_MAX)
            {
                continue;
            }
            if (isActionNode(i))
            {
                actions.push_back({inst.phase, i});
            }
        }

        std::stable_sort(
            actions.begin(), actions.end(),
            [](const ActionOrder &a, const ActionOrder &b)
            {
                return a.phase < b.phase;
            });

        for (const auto &a : actions)
        {
            m_graph.phase_order.push_back(a.index);
        }
    }

    // ─── Phase C: value order (topological sort) ──────────────────────────────

    void GraphBuilder::buildValueOrder()
    {

        const size_t n = m_graph.nodes.size();

        // Collect the value nodes and their dependencies.
        std::vector<NodeIndex> valueNodes;
        std::vector<std::vector<NodeIndex>> deps(n);

        for (NodeIndex i = 0; i < n; ++i)
        {
            if (!isValueNode(i))
            {
                continue;
            }
            valueNodes.push_back(i);

            const NodeInstance &inst = m_graph.nodes[i];
            for (uint32_t a = 0; a < inst.args_count; ++a)
            {
                const Arg &arg = m_graph.args[inst.args_offset + a];
                if (arg.kind == Arg::Kind::NodeRef)
                {
                    if (isValueNode(arg.node_ref))
                    {
                        deps[i].push_back(arg.node_ref);
                    }
                }
            }
        }

        // Kahn's algorithm over the value nodes.
        std::vector<NodeIndex> indegree(n, 0);
        for (NodeIndex v : valueNodes)
        {
            indegree[v] = static_cast<NodeIndex>(deps[v].size());
        }

        std::vector<NodeIndex> queue;
        for (NodeIndex v : valueNodes)
        {
            if (indegree[v] == 0)
            {
                queue.push_back(v);
            }
        }

        // A reverse adjacency: for each node d, which nodes depend on d?
        std::vector<std::vector<NodeIndex>> rdeps(n);
        for (NodeIndex v : valueNodes)
        {
            for (NodeIndex d : deps[v])
            {
                rdeps[d].push_back(v);
            }
        }

        std::vector<NodeIndex> order;
        while (!queue.empty())
        {
            const NodeIndex v = queue.back();
            queue.pop_back();
            order.push_back(v);

            for (NodeIndex dependent : rdeps[v])
            {
                if (--indegree[dependent] == 0)
                {
                    queue.push_back(dependent);
                }
            }
        }

        if (order.size() != valueNodes.size())
        {
            // A cycle exists among the value nodes.
            m_diag.errorAt(DiagCode::Type_Cycle, SourceLocation{1, 1},
                           "value nodes form a cycle; the graph's value "
                           "order cannot be computed");
            // Still record the partial order; the compile aborted on
            // errors anyway.
        }

        m_graph.value_order = std::move(order);
    }

    /// Build the zero value of a struct type, appending its bytes to
    /// the graph's struct-value pool. Returns a `Literal` of kind
    /// `Struct` holding the (offset, length) into the pool.
    ///
    /// Recurses: a struct field of struct type produces its own zero
    /// and its bytes are included in the parent's layout.
    ///
    /// Returns `Literal{}` if the type is not a struct, or if the
    /// struct's fields cannot be resolved.
    Literal GraphBuilder::zeroStructValue(const TypeId &type)
    {
        if (type.kind != TypeId::Kind::Struct)
            return Literal{};

        const StructTypeInfo *info = m_registry.findStruct(type.name);
        if (info == nullptr)
            return Literal{};

        const uint32_t offset =
            static_cast<uint32_t>(m_graph.struct_value_pool.size());

        for (const StructFieldInfo &field : info->fields)
        {
            appendZeroFieldBytes(field.type);
        }

        const uint32_t length =
            static_cast<uint32_t>(m_graph.struct_value_pool.size()) - offset;

        return makeStruct(offset, length);
    }

    /// Append the zero bytes of one field's type to the struct-value
    /// pool. Dispatches on the field type's kind.
    void GraphBuilder::appendZeroFieldBytes(const TypeId &type)
    {
        switch (type.kind)
        {
        case TypeId::Kind::Primitive:
        case TypeId::Kind::Enum:
        case TypeId::Kind::Handle:
        {
            // The field's zero is a Literal; append its wire bytes.
            const Literal zero = zeroLiteralOf(type, m_registry);
            appendLiteralWireBytes(zero);
            return;
        }

        case TypeId::Kind::Struct:
        {
            // Recurse: append the nested struct's zero.
            const StructTypeInfo *info = m_registry.findStruct(type.name);
            if (info == nullptr)
                return;
            for (const StructFieldInfo &sub : info->fields)
            {
                appendZeroFieldBytes(sub.type);
            }
            return;
        }

        case TypeId::Kind::Invalid:
            return;
        }
    }

    /// Append the wire-form bytes of a primitive/enum/handle literal
    /// to the struct-value pool. This is the same layout the serializer
    /// uses for a struct field; it must match byte-for-byte.
    void GraphBuilder::appendLiteralWireBytes(const Literal &lit)
    {
        // The exact byte layout matches `serialization::format`'s
        // primitives. In practice this is:
        //   bool  → 1 byte (0/1)
        //   char  → 1 byte
        //   int8  → 1 byte
        //   int16 → 2 bytes LE
        //   int32 → 4 bytes LE
        //   int64 → 8 bytes LE
        //   uint8..uint64 → same, unsigned
        //   float32 → 4 bytes LE IEEE-754
        //   float64 → 8 bytes LE IEEE-754
        //   enum  → 8 bytes LE (Int64)
        //   handle → 4 bytes LE (the handle's id; nil is 0)
        //
        // The graph builder writes these bytes directly. A helper
        // shared with the serializer would be cleaner; for now,
        // duplicate the layout.
        //
        // (See `appendZeroFieldBytes` callers — this is only used for
        // zeros, so a `zero` literal of any kind is written here.)
        switch (lit.kind)
        {
        case Literal::Kind::Nil:
            m_graph.struct_value_pool.push_back(0);
            return;
        case Literal::Kind::Bool:
            m_graph.struct_value_pool.push_back(lit.b ? 1 : 0);
            return;
        case Literal::Kind::Char:
            m_graph.struct_value_pool.push_back(static_cast<char>(lit.c));
            return;
        case Literal::Kind::Int8:
            m_graph.struct_value_pool.push_back(static_cast<char>(lit.i));
            return;
        case Literal::Kind::Int16:
        {
            const int16_t v = static_cast<int16_t>(lit.i);
            m_graph.struct_value_pool.push_back(static_cast<char>(v & 0xFF));
            m_graph.struct_value_pool.push_back(static_cast<char>((v >> 8) & 0xFF));
            return;
        }
        case Literal::Kind::Int32:
        {
            const int32_t v = static_cast<int32_t>(lit.i);
            for (int i = 0; i < 4; ++i)
                m_graph.struct_value_pool.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
            return;
        }
        case Literal::Kind::Int64:
        {
            const int64_t v = lit.i;
            for (int i = 0; i < 8; ++i)
                m_graph.struct_value_pool.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
            return;
        }
        case Literal::Kind::UInt8:
            m_graph.struct_value_pool.push_back(static_cast<char>(lit.u));
            return;
        case Literal::Kind::UInt16:
        {
            const uint16_t v = static_cast<uint16_t>(lit.u);
            m_graph.struct_value_pool.push_back(static_cast<char>(v & 0xFF));
            m_graph.struct_value_pool.push_back(static_cast<char>((v >> 8) & 0xFF));
            return;
        }
        case Literal::Kind::UInt32:
        {
            const uint32_t v = static_cast<uint32_t>(lit.u);
            for (int i = 0; i < 4; ++i)
                m_graph.struct_value_pool.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
            return;
        }
        case Literal::Kind::UInt64:
        {
            const uint64_t v = lit.u;
            for (int i = 0; i < 8; ++i)
                m_graph.struct_value_pool.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
            return;
        }
        case Literal::Kind::Float32:
        {
            uint32_t bits;
            std::memcpy(&bits, &lit.f, 4);
            for (int i = 0; i < 4; ++i)
                m_graph.struct_value_pool.push_back(static_cast<char>((bits >> (8 * i)) & 0xFF));
            return;
        }
        case Literal::Kind::Float64:
        {
            uint64_t bits;
            std::memcpy(&bits, &lit.f, 8);
            for (int i = 0; i < 8; ++i)
                m_graph.struct_value_pool.push_back(static_cast<char>((bits >> (8 * i)) & 0xFF));
            return;
        }
        case Literal::Kind::String:
        {
            // A string field's zero is an empty string. Its bytes are
            // (offset, length) into the graph's string pool; for the
            // zero, offset is the current end of the pool, length is 0.
            // The wire form for a struct field of string type is
            // (offset, length), 8 bytes total.
            const uint32_t off = static_cast<uint32_t>(m_graph.string_pool.size());
            for (int i = 0; i < 4; ++i)
                m_graph.struct_value_pool.push_back(static_cast<char>((off >> (8 * i)) & 0xFF));
            for (int i = 0; i < 4; ++i)
                m_graph.struct_value_pool.push_back(0);
            return;
        }
        case Literal::Kind::Struct:
            // Recursive case is handled by `appendZeroFieldBytes`.
            return;
        }
    }

    // ─── Helpers ──────────────────────────────────────────────────────────────

    bool GraphBuilder::isValueNode(NodeIndex idx) const
    {
        if (idx >= m_graph.nodes.size())
            return false;
        const uint32_t typeId = m_graph.nodes[idx].type_id;
        if (typeId == UINT32_MAX)
            return false;
        if (typeId >= m_registry.nodeTypes.size())
            return false;
        return m_registry.nodeTypes[typeId].kind == NodeKind::Value;
    }

    bool GraphBuilder::isActionNode(NodeIndex idx) const
    {
        if (idx >= m_graph.nodes.size())
            return false;
        const uint32_t typeId = m_graph.nodes[idx].type_id;
        if (typeId == UINT32_MAX)
            return false;
        if (typeId >= m_registry.nodeTypes.size())
            return false;
        return m_registry.nodeTypes[typeId].kind == NodeKind::Action;
    }

    /// Build an inline node's NodeInstance and append it to the
    /// graph. Returns the index of the newly-appended node.
    ///
    /// An inline node's arguments can themselves be inline nodes;
    /// this function recurses. The recursion is bounded by the
    /// source's nesting depth.
    NodeIndex GraphBuilder::buildInlineNode(const NodeExprAST *expr)
    {
        NodeInstance inst;

        // ─── type_id and phase ─────────────────────────────────────────
        if (expr->type != nullptr)
        {
            const std::string_view typeName =
                m_diag.stringPool()
                    ? m_diag.stringPool()->lookupView(expr->type->name)
                    : std::string_view{};
            inst.type_id = nodeTypeIndex(m_registry, typeName);

            const NodeTypeInfo *info = nullptr;
            for (const auto &ni : m_registry.nodeTypes)
            {
                if (ni.name == typeName)
                {
                    info = &ni;
                    break;
                }
            }
            if (info != nullptr)
            {
                inst.phase = info->phase;
            }
        }

        // ─── args ──────────────────────────────────────────────────────
        // Same buffering pattern as buildNode: build into a
        // temporary, then append contiguously.
        std::vector<Arg> tempArgs;
        tempArgs.reserve(expr->args.size());
        for (BaseAST *arg : expr->args)
        {
            tempArgs.push_back(buildArg(arg));
        }

        inst.args_offset = static_cast<uint32_t>(m_graph.args.size());
        for (Arg &a : tempArgs)
        {
            m_graph.args.push_back(std::move(a));
        }
        inst.args_count = static_cast<uint32_t>(tempArgs.size());

        // ─── append and return index ───────────────────────────────────
        const NodeIndex idx = static_cast<NodeIndex>(m_graph.nodes.size());
        m_graph.nodes.push_back(inst);
        return idx;
    }

    // ─── Public entry points ──────────────────────────────────────────────────

    std::unique_ptr<Graph> buildGraphFromModules(
        const std::vector<ModuleContext> &modules,
        const Registry &registry,
        StringPool &pool,
        DiagnosticEngine &diag)
    {
        GraphBuilder builder(registry, pool, diag);
        return builder.build(modules);
    }

    std::unique_ptr<Graph> buildGraph(const ModuleAST *module,
                                      const SymbolTable &symbols,
                                      const ResolutionMap &resolutions,
                                      const ConstantValueMap &constants,
                                      const TypeMap &types,
                                      const Registry &registry,
                                      StringPool &pool,
                                      DiagnosticEngine &diag)
    {
        // Wrap the single-module inputs into a ModuleContext and delegate.
        // const_cast is safe: the graph builder does not mutate the AST.
        ModuleContext ctx{
            const_cast<ModuleAST *>(module),
            &symbols,
            &resolutions,
            &constants,
            &types,
        };
        return buildGraphFromModules({ctx}, registry, pool, diag);
    }

} // namespace lucid::sema