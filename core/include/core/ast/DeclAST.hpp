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
///   - ImportDeclAST       `import module_path`
///   - EnumDeclAST         `enum NAME { members }`
///   - EnumMemberAST       one member of an enum
///   - ResourceDeclAST     `resource NAME { fields }`
///   - ResourceFieldAST    `name: type [ = value ]` (inside a resource)
///   - NodeDeclAST         `node NAME = node_expr [ on triggers ]`
///
/// ─── The parser does not validate ─────────────────────────────────────────
/// Every check that requires knowing the grammar's rules beyond syntax is
/// deferred to Sema: duplicate enum members, duplicate fields, unknown
/// attributes, unknown node types, unknown trigger names, and so on.
///
/// ─── Attributes on every declaration ──────────────────────────────────────
/// The grammar's §2.3 allows an attribute list on any of the four
/// top-level declarations. The parser accepts attributes anywhere the
/// grammar allows them and does not check which attribute names are
/// recognized. Sema enforces both the set of recognized attributes
/// (`@export` only) and the declarations they may appear on (`resource`
/// only).

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

/// `import module_path`
///
/// `path` is the dotted module path as written (`core.keys`). `name` is
/// the module name, which is always the final path segment (`keys`). The
/// parser computes `name` from `path`; there is no `as` clause and no
/// alias.
///
/// The module name is used by Sema as the `::` qualifier in type
/// positions. It is not a value-position name; a value reaches a
/// declaration by the bare name the import injects, not through the
/// module name. See the grammar's §2.2 for the import model.
///
/// The parser does not check that `path` resolves to a file. Import
/// resolution is the driver's job; `Import_ModuleNotFound` is reported by
/// the resolver, not the parser.
struct ImportDeclAST : DeclAST
{
    static constexpr ASTKind staticKind = ASTKind::ImportDecl;

    /// The dotted module path, as written. `core.keys` is a three-segment
    /// name spelled with dots; the segments are not stored separately.
    InternedString path;

    ImportDeclAST() : DeclAST(ASTKind::ImportDecl, InternedString{}) {}

    ImportDeclAST(InternedString p,
                  InternedString moduleName,
                  ArenaSpan<AttributeAST *> attrs = {})
        : DeclAST(ASTKind::ImportDecl, moduleName, attrs), path(p) {}

    /// True if the import path has more than one segment (`core.keys`).
    /// Used by diagnostics that want to name the module by its full path
    /// rather than by its final segment.
    bool isDotted() const { return path.isValid(); }
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

/// @brief One field of a resource declaration.
///
/// The grammar's §2.5 writes:
///
///     resource_field ::= IDENTIFIER ':' type_id [ '=' value ]
///
/// The default is a `value`, not only a literal. The four value forms are
/// all legal here, so `defaultValue` is a `BaseAST*` and not a
/// `LiteralValueAST*`. A default like `Key.A` is a `FieldAccessValueAST`;
/// a default like `10` is a `LiteralValueAST`; a default like
/// `Float32Node(1.0)` is an `InlineNodeValueAST`.
///
/// `hasDefault()` distinguishes "no default written" from "default written
/// as the zero value of the type." A field with no default is
/// zero-initialized by Sema; a field with an explicit default uses that
/// default, even if the default happens to be the zero value.
struct ResourceFieldAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::ResourceField;

    InternedString name;
    TypeIdAST *type = nullptr;
    BaseAST *defaultValue = nullptr;

    ResourceFieldAST() : BaseAST(ASTKind::ResourceField) {}

    ResourceFieldAST(InternedString n,
                     TypeIdAST *t,
                     BaseAST *d = nullptr)
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

/// `node NAME = node_expr [ on trigger_list ]`
///
/// The `on` clause's targets are stored as a span of identifiers. Each
/// target is resolved by Sema against the set of trigger nodes in scope.
/// The parser does not check that the target names anything; it stores the
/// spellings.
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