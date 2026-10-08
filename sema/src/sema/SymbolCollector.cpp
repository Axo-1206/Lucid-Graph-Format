/// @file sema/src/sema/SymbolCollector.cpp
///
/// @brief Implementation of Pass 1's symbol collector.

#include "SymbolCollector.hpp"

namespace lucid::sema
{

    namespace
    {

        /// Map a declaration kind to a symbol kind. Returns false if the
        /// declaration is not a symbol (e.g., an UnknownAST from a parse
        /// error).
        bool symbolKindOf(ASTKind kind, SymbolKind& out) noexcept
        {
            switch (kind)
            {
            case ASTKind::ImportDecl:
                out = SymbolKind::Import;
                return true;
            case ASTKind::EnumDecl:
                out = SymbolKind::Enum;
                return true;
            case ASTKind::ResourceDecl:
                out = SymbolKind::Resource;
                return true;
            case ASTKind::NodeDecl:
                out = SymbolKind::Node;
                return true;
            case ASTKind::CompositeDecl:
                out = SymbolKind::Composite;
                return true;
            default:
                return false;
            }
        }

    } // namespace

    void collectSymbols(const ModuleAST* module,
                        SymbolTable& table,
                        lucid::diag::DiagnosticEngine& diag)
    {
        if (module == nullptr)
        {
            return;
        }

        for (DeclAST* decl : module->decls)
        {
            if (decl == nullptr)
            {
                continue;
            }

            // Skip declarations that are not symbols. An UnknownAST
            // (from a parse error) has no name and no kind we can use.
            SymbolKind kind;
            if (!symbolKindOf(decl->kind, kind))
            {
                continue;
            }

            // Skip symbols with an invalid name. The parser produces a
            // valid name on a successful parse; a parse error can leave
            // the name invalid. Reporting a symbol with no name would
            // produce confusing diagnostics.
            if (!decl->name.isValid())
            {
                continue;
            }

            table.add(decl->name, kind, decl, diag);
        }
    }

} // namespace lucid::sema
