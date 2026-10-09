/// @file sema/src/sema/DeclHelpers.hpp
///
/// @brief Small helpers shared between Sema passes.

#pragma once

#include "core/ast/AttributeAST.hpp"
#include "core/ast/DeclAST.hpp"
#include "core/memory/InternedString.hpp"
#include "sema/SymbolTable.hpp"

namespace lucid::sema
{

    /// @brief Map a declaration kind to a symbol kind. Returns false if
    ///        the declaration is not a symbol.
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

    /// @brief True if the declaration has the given attribute.
    inline bool hasAttribute(const DeclAST *decl,
                             InternedString attributeName) noexcept
    {
        if (decl == nullptr)
            return false;
        for (AttributeAST *attr : decl->attributes)
        {
            if (attr != nullptr && attr->name == attributeName)
                return true;
        }
        return false;
    }

} // namespace lucid::sema