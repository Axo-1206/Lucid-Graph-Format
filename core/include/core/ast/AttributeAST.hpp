/// @file core/ast/AttributeAST.hpp
///
/// @brief The AST node for one attribute prefix: `@name`.
///
/// ─── What an attribute is ─────────────────────────────────────────────────
/// The grammar's §2.3 writes:
///
///     attribute_list ::= { '@' IDENTIFIER }
///
/// An attribute list is a sequence of `@name` prefixes. The only
/// recognized attribute is `@export`, which controls whether a resource
/// declaration is visible outside its module. A declaration may be
/// preceded by any number of attributes.
///
/// ─── Where attributes may appear ──────────────────────────────────────────
/// The grammar allows an attribute list on any of the four top-level
/// declarations:
///
///     import_decl   ::= attribute_list 'import'   ...
///     enum_decl     ::= attribute_list 'enum'     ...
///     resource_decl ::= attribute_list 'resource' ...
///     node_decl     ::= attribute_list 'node'     ...
///
/// The parser accepts any `@name` in any of those positions. It does not
/// know the set of recognized attributes and does not enforce where they
/// may appear.
///
/// ─── What Sema enforces ───────────────────────────────────────────────────
/// Sema enforces two rules:
///
///   1. The set of recognized attributes is exactly { @export }.
///      Any other `@name` is `Attr_Unknown`.
///
///   2. `@export` is meaningful on `resource` only. Syntactically it may
///      precede `import`, `enum`, or `node`; semantically it is an error
///      on any of them (`Attr_ExportOnImport`, `Attr_ExportOnEnum`,
///      `Attr_ExportOnNode`). An `@export` inside a declaration body is
///      `Attr_ExportOnField`. The same attribute twice on one declaration
///      is `Attr_Duplicate`.
///
/// ─── No argument list ─────────────────────────────────────────────────────
/// The grammar allows `@IDENTIFIER` only, with no parenthesized arguments.
/// Every recognized attribute takes no arguments. `@deprecate` is not part
/// of the format; a future deprecation mechanism is handled by a special
/// comment convention or by a future attribute, deferred until needed. If
/// a future attribute needs arguments, this node grows a field and the
/// parser's attribute rule grows the corresponding parse.
///
/// ─── Location convention ──────────────────────────────────────────────────
/// The node's `loc` is the location of the `@` sign, not the identifier
/// that follows it. This makes a diagnostic that points at the attribute
/// point at the whole `@export` prefix, which is what a reader expects.

#pragma once

#include "core/ast/BaseAST.hpp"
#include "core/memory/InternedString.hpp"

/// @brief One attribute prefix: `@name`.
///
/// The parser produces one of these per `@` in the source. The `name`
/// field is the identifier that follows the `@`, without the `@`.
///
/// For `@export`:
///     name = "export"
///
/// For an unrecognized attribute, the node is still produced; Sema
/// rejects it later.
struct AttributeAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::Attribute;

    /// The identifier after the `@`, without the `@`.
    InternedString name;

    AttributeAST() : BaseAST(ASTKind::Attribute) {}

    explicit AttributeAST(InternedString n)
        : BaseAST(ASTKind::Attribute), name(n) {}
};