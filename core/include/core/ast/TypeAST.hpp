/// @file core/ast/TypeAST.hpp
///
/// @brief The AST node for a type reference: `type_id`.
///
/// ─── What a type reference is ─────────────────────────────────────────────
/// The grammar's §2.5 writes:
///
///     type_id ::= [ IDENTIFIER '::' ] IDENTIFIER
///
/// A type reference is a name, optionally with one level of module
/// qualification. `Key` refers to a type in the local scope;
/// `core::Key` refers to a type from the module imported as `core`.
///
/// ─── The separator is '::', not '.' ───────────────────────────────────────
/// The dot is field access (`Key.W`). The double-colon is module
/// qualification (`core::Key`). The two never mix in one name, and the
/// two tokens are distinct in the lexer, so the parser never has to
/// decide which meaning a separator carries.
///
/// ─── What a type reference is not ─────────────────────────────────────────
/// There is no syntax for primitives, arrays, function types, or
/// nullability. A type name resolves against the host registry: whether
/// `float` is a primitive or `BodyRef` is a handle is a fact the registry
/// declares, not a fact the AST carries. The formatter emits the name it
/// read; Sema resolves the name.
///
/// ─── Depth ────────────────────────────────────────────────────────────────
/// The grammar allows at most one level of qualification. `a::b::c` is not
/// a valid type reference. This is a known limitation (see §4.1 of the
/// grammar). If the grammar later allows deeper paths, this node's
/// `qualifier` field becomes a span of path segments; the `name` field
/// stays, and callers that only care about the final segment are unchanged.

#pragma once

#include "core/ast/BaseAST.hpp"
#include "core/memory/InternedString.hpp"

/// @brief A type reference: `IDENTIFIER` or `IDENTIFIER '::' IDENTIFIER`.
///
///   TypeIdAST{name = "Key"}                      -- `Key`
///   TypeIdAST{qualifier = "core", name = "Key"}  -- `core::Key`
///
/// The `qualifier` field is invalid when there is no `::`. The `name`
/// field is always valid after construction.
struct TypeIdAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::TypeId;

    /// The module qualifier, if any. Invalid when the type reference is
    /// unqualified.
    InternedString qualifier;

    /// The type's name. Valid in every construction path.
    InternedString name;

    TypeIdAST() : BaseAST(ASTKind::TypeId) {}

    /// Construct an unqualified type reference.
    explicit TypeIdAST(InternedString n)
        : BaseAST(ASTKind::TypeId), name(n) {}

    /// Construct a qualified type reference.
    TypeIdAST(InternedString q, InternedString n)
        : BaseAST(ASTKind::TypeId), qualifier(q), name(n) {}

    // ─── Queries ────────────────────────────────────────────────────────

    /// True if the type reference has a module qualifier (`core::Key`).
    bool isQualified() const { return qualifier.isValid(); }

    /// True if the type reference has no qualifier (`Key`).
    bool isSimple() const { return !isQualified(); }
};