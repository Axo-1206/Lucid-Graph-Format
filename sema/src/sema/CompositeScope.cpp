/// @file sema/src/sema/CompositeScope.cpp
///
/// @brief Implementation of CompositeScope.

#include "sema/CompositeScope.hpp"

#include "core/diagnostics/DiagCode.hpp"

namespace lucid::sema
{

    bool CompositeScope::addInput(CompositeInputAST *input,
                                  lucid::diag::DiagnosticEngine &diag)
    {
        if (input == nullptr)
            return false;
        if (!input->name.isValid())
            return false;

        return m_local.add(input->name,
                           SymbolKind::CompositeInput,
                           input,
                           diag);
    }

    bool CompositeScope::addLocal(InternedString name,
                                  SymbolKind kind,
                                  BaseAST *decl,
                                  lucid::diag::DiagnosticEngine &diag)
    {
        if (!name.isValid())
            return false;
        return m_local.add(name, kind, decl, diag);
    }

    const Symbol *CompositeScope::find(InternedString name,
                                       const SymbolTable &moduleSymbols) const noexcept
    {
        const Symbol *local = m_local.find(name);
        if (local != nullptr)
            return local;
        return moduleSymbols.find(name);
    }

    namespace
    {

        bool bodyDeclSymbolKind(ASTKind kind, SymbolKind &out) noexcept
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
            default:
                return false;
            }
        }

    } // namespace

    CompositeScope buildCompositeScope(const CompositeDeclAST *decl,
                                       lucid::diag::DiagnosticEngine &diag)
    {
        CompositeScope scope;

        if (decl == nullptr)
            return scope;

        for (CompositeInputAST *input : decl->inputs)
        {
            scope.addInput(input, diag);
        }

        for (DeclAST *bodyDecl : decl->body)
        {
            if (bodyDecl == nullptr)
                continue;

            SymbolKind kind;
            if (!bodyDeclSymbolKind(bodyDecl->kind, kind))
                continue;
            if (!bodyDecl->name.isValid())
                continue;

            scope.addLocal(bodyDecl->name, kind, bodyDecl, diag);
        }

        return scope;
    }

} // namespace lucid::sema