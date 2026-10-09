/// @file sema/src/sema/DeadCodeChecker.cpp
///
/// @brief Implementation of Pass 5: dead-code detection.

#include "DeadCodeChecker.hpp"

#include "core/ast/DeclAST.hpp"
#include "core/ast/ValueAST.hpp"
#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/StringPool.hpp"

#include <string_view>
#include <unordered_set>

using namespace lucid::diag;

namespace lucid::sema
{

    namespace
    {

        /// Find a node type by name in the registry. Returns nullptr if
        /// not found.
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

        /// True if the node declaration's type is a Value node.
        bool isValueNode(const NodeDeclAST *decl,
                         const Registry &registry,
                         const StringPool *pool) noexcept
        {
            if (decl == nullptr || decl->expr == nullptr ||
                decl->expr->type == nullptr)
            {
                return false;
            }
            if (pool == nullptr)
                return false;

            const std::string_view typeName =
                pool->lookupView(decl->expr->type->name);
            const NodeTypeInfo *info = lookupNodeType(registry, typeName);
            if (info == nullptr)
                return false;
            return info->kind == NodeKind::Value;
        }

    } // namespace

    // ─── Public entry point ───────────────────────────────────────────────────

    void checkDeadCode(const ModuleAST *module,
                       const ResolutionMap &resolutions,
                       const Registry &registry,
                       DiagnosticEngine &diag)
    {
        if (module == nullptr)
            return;

        const StringPool *pool = diag.stringPool();

        // ─── Pass A: collect referenced node declarations ──────────────────
        //
        // A node declaration is "referenced" if any node's argument is
        // an identifier that resolves to it. The set is keyed on the
        // NodeDeclAST pointer.
        std::unordered_set<const NodeDeclAST *> referenced;

        for (DeclAST *decl : module->decls)
        {
            if (decl == nullptr || decl->kind != ASTKind::NodeDecl)
            {
                continue;
            }
            const auto *nodeDecl = decl->as<NodeDeclAST>();
            if (nodeDecl->expr == nullptr)
                continue;

            for (BaseAST *arg : nodeDecl->expr->args)
            {
                if (arg == nullptr)
                    continue;
                if (arg->kind != ASTKind::IdentifierValue)
                    continue;

                const BaseAST *target = resolutions.lookup(arg);
                if (target == nullptr)
                    continue;
                if (target->kind != ASTKind::NodeDecl)
                    continue;

                referenced.insert(target->as<NodeDeclAST>());
            }
        }

        // ─── Pass B: report unreferenced Value nodes ───────────────────────
        for (DeclAST *decl : module->decls)
        {
            if (decl == nullptr || decl->kind != ASTKind::NodeDecl)
            {
                continue;
            }
            const auto *nodeDecl = decl->as<NodeDeclAST>();

            // Only Value nodes are candidates.
            if (!isValueNode(nodeDecl, registry, pool))
            {
                continue;
            }

            if (referenced.find(nodeDecl) != referenced.end())
            {
                continue;
            }

            // The node is unused. Report a warning.
            diag.warning(DiagCode::Warn_DeadNode, const_cast<NodeDeclAST *>(nodeDecl),
                         "value node '",
                         pool ? pool->lookupView(nodeDecl->name)
                              : std::string_view{"<unknown>"},
                         "' is never used");
        }
    }

} // namespace lucid::sema