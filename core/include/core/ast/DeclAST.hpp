/// @file core/ast/DeclAST.hpp
///
/// @brief The AST nodes for the five top-level declarations and the
///        fields that appear inside them.
///
/// ─── The five declarations ────────────────────────────────────────────────
/// The grammar's §2.1 writes:
///
///     top_decl ::= import_decl
///                | enum_decl
///                | resource_decl
///                | node_decl
///                | composite_decl
///
/// Every declaration carries the same base fields (a name, an attribute
/// list, and a source location), plus its own. The family base DeclAST
/// holds the shared fields; each concrete declaration adds its own.
///
/// ─── What lives here ──────────────────────────────────────────────────────
///   - DeclAST             family base
///   - ImportDeclAST       `import module_path [ as IDENTIFIER ]`
///   - EnumDeclAST         `enum NAME { members }`
///   - ResourceDeclAST     `resource NAME { fields }`
///   - ResourceFieldAST    `name: type [ = literal ]` (inside a resource)
///   - NodeDeclAST         `node NAME = node_expr [ on triggers ]`
///   - CompositeDeclAST    `composite NAME { input/output/body }`
///   - CompositeInputAST   `name: type` (inside a composite's input block)
///   - CompositeOutputAST  `name: type = value` (inside a composite's output block)
///
/// ─── The parser does not validate ─────────────────────────────────────────
/// Every check that requires knowing the grammar's rules beyond syntax is
/// deferred to Sema: duplicate enum members, duplicate fields, unknown
/// attributes, unknown node types, unknown trigger names, `Event` in the
/// wrong position, cycles among composites, and so on. This file defines
/// the shape; the checks land in sema/.

#pragma once

#include "core/ast/BaseAST.hpp"
#include "core/ast/AttributeAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/ast/ValueAST.hpp" // NodeExprAST, LiteralValueAST
#include "core/memory/ArenaSpan.hpp"
#include "core/memory/InternedString.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// DeclAST — family base
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Base for every declaration node.
///
/// Every declaration has a name, an attribute list, and a location. The
/// name means different things for different declarations:
///
///   - ImportDeclAST:      the local alias (the last segment of the path,
///                         or the alias after `as`).
///   - EnumDeclAST:        the enum's identifier.
///   - ResourceDeclAST:    the resource's identifier.
///   - NodeDeclAST:        the node's identifier.
///   - CompositeDeclAST:   the composite's identifier.
///
/// The `attributes` span is empty when the declaration has no attributes.
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

/// @brief An import declaration: `import module_path [ as IDENTIFIER ]`.
///
/// The grammar's §2.2 writes:
///
///     import_decl ::= 'import' module_path [ 'as' IDENTIFIER ]
///     module_path ::= IDENTIFIER { '.' IDENTIFIER }
///
/// The `path` field is the full dotted path as a single interned string
/// ("core.keys"). The base's `name` field is the local alias: the last
/// segment of the path by default, or the identifier after `as`.
///
/// The parser does not resolve the path. The CLI's import linker reads it,
/// loads the module, and binds the alias. Sema consumes the result.
struct ImportDeclAST : DeclAST
{
    static constexpr ASTKind staticKind = ASTKind::ImportDecl;

    /// The dotted module path as a single interned string ("core.keys").
    InternedString path;

    ImportDeclAST() : DeclAST(ASTKind::ImportDecl, InternedString{}) {}

    ImportDeclAST(InternedString p,
                  InternedString alias,
                  ArenaSpan<AttributeAST *> attrs = {})
        : DeclAST(ASTKind::ImportDecl, alias, attrs), path(p) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// EnumDeclAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief An enum declaration: `enum NAME { members }`.
///
/// The grammar's §2.4 writes:
///
///     enum_decl ::= 'enum' IDENTIFIER '{' enum_member_list '}'
///     enum_member_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]
///
/// Members are an ordered span of names. The parser does not check for
/// duplicates; Sema reports a duplicate member as `Name_DuplicateEnumMember`.
///
/// The grammar notes (§2.4) that enum declarations are "typically
/// host-provided" and that the host registry treats them as authoritative.
/// A script-declared enum whose name is not in the registry is a Sema
/// concern, not a parser concern; the parser accepts the syntax.
struct EnumDeclAST : DeclAST
{
    static constexpr ASTKind staticKind = ASTKind::EnumDecl;

    ArenaSpan<InternedString> members;

    EnumDeclAST() : DeclAST(ASTKind::EnumDecl, InternedString{}) {}

    EnumDeclAST(InternedString n,
                ArenaSpan<InternedString> m,
                ArenaSpan<AttributeAST *> attrs = {})
        : DeclAST(ASTKind::EnumDecl, n, attrs), members(m) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// ResourceFieldAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief One field inside a resource declaration.
///
/// The grammar's §2.5 writes:
///
///     resource_field ::= IDENTIFIER ':' type_id [ '=' literal ]
///
/// The default is a literal, not a general value: the grammar restricts
/// the right-hand side of a field default to `literal`. A field with no
/// default has `defaultValue == nullptr`.
struct ResourceFieldAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::ResourceField;

    InternedString name;

    /// The field's declared type. Non-null after a successful parse.
    TypeIdAST *type = nullptr;

    /// The field's default value, or null when there is none.
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

/// @brief A resource declaration: `resource NAME { fields }`.
///
/// The grammar's §2.5 writes:
///
///     resource_decl ::= attribute_list 'resource' IDENTIFIER '{'
///                          { resource_field }
///                      '}'
///
/// A resource is a named set of typed fields. Every field has a name and
/// a type; a field may have a default.
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

/// @brief A node declaration:
///
///     `node NAME = node_expr [ on trigger_list ]`
///
/// The grammar's §2.6 writes:
///
///     node_decl    ::= 'node' IDENTIFIER '=' node_expr [ 'on' trigger_list ]
///     trigger_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]
///
/// The right-hand side is a NodeExprAST. The trigger list is a span of
/// names, empty when there is no `on` clause.
struct NodeDeclAST : DeclAST
{
    static constexpr ASTKind staticKind = ASTKind::NodeDecl;

    /// The right-hand side's node expression. Non-null after a successful
    /// parse.
    NodeExprAST *expr = nullptr;

    /// The trigger names from the `on` clause, in source order. Empty when
    /// there is no `on` clause.
    ArenaSpan<InternedString> triggers;

    NodeDeclAST() : DeclAST(ASTKind::NodeDecl, InternedString{}) {}

    NodeDeclAST(InternedString n,
                NodeExprAST *e,
                ArenaSpan<InternedString> t = {},
                ArenaSpan<AttributeAST *> attrs = {})
        : DeclAST(ASTKind::NodeDecl, n, attrs), expr(e), triggers(t) {}

    bool hasTriggers() const { return !triggers.empty(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// CompositeInputAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief One field inside a composite's input block.
///
/// The grammar's §2.7 writes:
///
///     composite_field ::= IDENTIFIER ':' type_id
///
/// An input has a name and a type. There is no default; every composite
/// use must supply every input. (This is a deliberate grammar choice,
/// noted in §8.6.)
struct CompositeInputAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::CompositeInput;

    InternedString name;
    TypeIdAST *type = nullptr;

    CompositeInputAST() : BaseAST(ASTKind::CompositeInput) {}

    CompositeInputAST(InternedString n, TypeIdAST *t)
        : BaseAST(ASTKind::CompositeInput), name(n), type(t) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// CompositeOutputAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief One field inside a composite's output block.
///
/// The grammar's §2.7 writes:
///
///     composite_output ::= IDENTIFIER ':' type_id '=' value
///
/// An output has a name, a type, and a right-hand side that refers to
/// something inside the composite body. The right-hand side is a value
/// node (one of the four value forms).
///
/// Whether the output is a data output (int, float, handle, array) or an
/// event output (Event) is determined by the type, and by where the
/// output appears in the graph. Sema checks that the type is legal and
/// that the right-hand side matches.
struct CompositeOutputAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::CompositeOutput;

    InternedString name;
    TypeIdAST *type = nullptr;
    BaseAST *value = nullptr; // one of the four value nodes

    CompositeOutputAST() : BaseAST(ASTKind::CompositeOutput) {}

    CompositeOutputAST(InternedString n, TypeIdAST *t, BaseAST *v)
        : BaseAST(ASTKind::CompositeOutput), name(n), type(t), value(v) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// CompositeDeclAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief A composite declaration:
///
///     @attrs composite NAME { input ... output ... body ... }
///
/// The grammar's §2.7 writes:
///
///     composite_decl ::= attribute_list 'composite' IDENTIFIER '{'
///                          [ input_block ]
///                          [ output_block ]
///                          { composite_body_decl }
///                        '}'
///
/// The input and output blocks are optional. The body may contain imports,
/// enums, resources, and nodes. A composite cannot contain another
/// composite; the AST does not enforce this, but Sema does.
struct CompositeDeclAST : DeclAST
{
    static constexpr ASTKind staticKind = ASTKind::CompositeDecl;

    /// The input block's fields. Empty when there is no input block.
    ArenaSpan<CompositeInputAST *> inputs;

    /// The output block's fields. Empty when there is no output block.
    ArenaSpan<CompositeOutputAST *> outputs;

    /// The body's declarations: imports, enums, resources, nodes.
    ArenaSpan<DeclAST *> body;

    CompositeDeclAST() : DeclAST(ASTKind::CompositeDecl, InternedString{}) {}

    CompositeDeclAST(InternedString n,
                     ArenaSpan<CompositeInputAST *> in,
                     ArenaSpan<CompositeOutputAST *> out,
                     ArenaSpan<DeclAST *> b,
                     ArenaSpan<AttributeAST *> attrs = {})
        : DeclAST(ASTKind::CompositeDecl, n, attrs),
          inputs(in), outputs(out), body(b) {}

    bool hasInputs() const { return !inputs.empty(); }
    bool hasOutputs() const { return !outputs.empty(); }
};