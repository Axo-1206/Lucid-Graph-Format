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
/// recognized attribute is `@export`, which controls whether a declaration
/// is visible outside its module. A declaration may be preceded by any
/// number of attributes.
///
/// ─── The parser does not validate ─────────────────────────────────────────
/// The parser accepts any identifier after `@`. It does not know the set
/// of recognized attributes and does not enforce where they may appear.
/// Sema does both: it reports an unknown attribute (`Attr_Unknown`) and an
/// attribute in an illegal position (`Attr_ExportOnImport`,
/// `Attr_ExportOnField`, ...).
///
/// ─── No argument list ─────────────────────────────────────────────────────
/// The grammar allows `@IDENTIFIER` only, with no parenthesized arguments.
/// Every recognized attribute takes no arguments. If a future attribute
/// needs arguments, this node grows a field and the parser's attribute
/// rule grows the corresponding parse.
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