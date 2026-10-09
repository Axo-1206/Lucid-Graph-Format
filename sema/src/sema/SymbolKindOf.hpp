#pragma once
#include "core/ast/DeclAST.hpp"
#include "sema/SymbolTable.hpp"

namespace lucid::sema
{

    inline bool symbolKindOf(ASTKind kind, SymbolKind &out) noexcept
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

} // namespace lucid::sema