/// @file core/ast/DeclAST.hpp
///
/// @brief The AST nodes for the four top-level declarations and the
///        fields that appear inside them.
///
/// ─── The four declarations ────────────────────────────────────────────────
/// The grammar's §2.1 writes:
///
///     top_decl ::= import_decl
///                | enum_decl
///                | resource_decl
///                | node_decl
///
/// Every declaration carries the same base fields (a name, an attribute
/// list, and a source location), plus its own. The family base DeclAST
/// holds the shared fields; each concrete declaration adds its own.
///
/// ─── What lives here ──────────────────────────────────────────────────────
///   - DeclAST             family base
///   - ImportDeclAST       `import module_path [ as IDENTIFIER ]`
///   - EnumDeclAST         `enum NAME { members }`
///   - EnumMemberAST       one member of an enum
///   - ResourceDeclAST     `resource NAME { fields }`
///   - ResourceFieldAST    `name: type [ = literal ]` (inside a resource)
///   - NodeDeclAST         `node NAME = node_expr [ on triggers ]`
///
/// ─── The parser does not validate ─────────────────────────────────────────
/// Every check that requires knowing the grammar's rules beyond syntax is
/// deferred to Sema: duplicate enum members, duplicate fields, unknown
/// attributes, unknown node types, unknown trigger names, and so on.

#pragma once

#include "core/ast/BaseAST.hpp"
#include "core/ast/AttributeAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/ast/ValueAST.hpp"
#include "core/memory/ArenaSpan.hpp"
#include "core/memory/InternedString.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// DeclAST — family base
// ─────────────────────────────────────────────────────────────────────────────

struct DeclAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::Decl;

    InternedString name;

    ArenaSpan<AttributeAST *> attributes;

    explicit DeclAST(ASTKind k, InternedString n)
        : BaseAST(k), name(n) {}

    DeclAST(ASTKind k, InternedString n, ArenaSpan<AttributeAST *> attrs)
        : BaseAST(k), name(n), attributes(attrs) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// ImportDeclAST
// ─────────────────────────────────────────────────────────────────────────────

struct ImportDeclAST : DeclAST
{
    static constexpr ASTKind staticKind = ASTKind::ImportDecl;

    InternedString path;

    ImportDeclAST() : DeclAST(ASTKind::ImportDecl, InternedString{}) {}

    ImportDeclAST(InternedString p,
                  InternedString alias,
                  ArenaSpan<AttributeAST *> attrs = {})
        : DeclAST(ASTKind::ImportDecl, alias, attrs), path(p) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// EnumMemberAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief One member of an enum declaration.
struct EnumMemberAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::EnumMember;

    InternedString name;

    EnumMemberAST() : BaseAST(ASTKind::EnumMember) {}

    explicit EnumMemberAST(InternedString n)
        : BaseAST(ASTKind::EnumMember), name(n) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// EnumDeclAST
// ─────────────────────────────────────────────────────────────────────────────

struct EnumDeclAST : DeclAST
{
    static constexpr ASTKind staticKind = ASTKind::EnumDecl;

    ArenaSpan<EnumMemberAST *> members;

    EnumDeclAST() : DeclAST(ASTKind::EnumDecl, InternedString{}) {}

    EnumDeclAST(InternedString n,
                ArenaSpan<EnumMemberAST *> m,
                ArenaSpan<AttributeAST *> attrs = {})
        : DeclAST(ASTKind::EnumDecl, n, attrs), members(m) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// ResourceFieldAST
// ─────────────────────────────────────────────────────────────────────────────

struct ResourceFieldAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::ResourceField;

    InternedString name;
    TypeIdAST *type = nullptr;
    LiteralValueAST *defaultValue = nullptr;

    ResourceFieldAST() : BaseAST(ASTKind::ResourceField) {}

    ResourceFieldAST(InternedString n,
                     TypeIdAST *t,
                     LiteralValueAST *d = nullptr)
        : BaseAST(ASTKind::ResourceField), name(n), type(t), defaultValue(d) {}

    bool hasDefault() const { return defaultValue != nullptr; }
};

// ─────────────────────────────────────────────────────────────────────────────
// ResourceDeclAST
// ─────────────────────────────────────────────────────────────────────────────

struct ResourceDeclAST : DeclAST
{
    static constexpr ASTKind staticKind = ASTKind::ResourceDecl;

    ArenaSpan<ResourceFieldAST *> fields;

    ResourceDeclAST() : DeclAST(ASTKind::ResourceDecl, InternedString{}) {}

    ResourceDeclAST(InternedString n,
                    ArenaSpan<ResourceFieldAST *> f,
                    ArenaSpan<AttributeAST *> attrs = {})
        : DeclAST(ASTKind::ResourceDecl, n, attrs), fields(f) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// NodeDeclAST
// ─────────────────────────────────────────────────────────────────────────────

struct NodeDeclAST : DeclAST
{
    static constexpr ASTKind staticKind = ASTKind::NodeDecl;

    NodeExprAST *expr = nullptr;
    ArenaSpan<InternedString> triggers;

    NodeDeclAST() : DeclAST(ASTKind::NodeDecl, InternedString{}) {}

    NodeDeclAST(InternedString n,
                NodeExprAST *e,
                ArenaSpan<InternedString> t = {},
                ArenaSpan<AttributeAST *> attrs = {})
        : DeclAST(ASTKind::NodeDecl, n, attrs), expr(e), triggers(t) {}

    bool hasTriggers() const { return !triggers.empty(); }
};