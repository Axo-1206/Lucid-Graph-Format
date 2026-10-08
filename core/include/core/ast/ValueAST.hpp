/// @file core/ast/ValueAST.hpp
///
/// @brief The AST nodes for the four value forms.
///
/// ─── What a value is ──────────────────────────────────────────────────────
/// The grammar's §2.7 writes:
///
///     value ::= literal
///             | IDENTIFIER
///             | IDENTIFIER '.' IDENTIFIER
///             | node_expr
///
/// A value is what may appear as a node argument, or as the default
/// of a resource field. The four forms are exhaustive: there is no
/// operator grammar, no expression grammar, no parentheses-for-grouping.
///
/// ─── The four value nodes ─────────────────────────────────────────────────
///   - LiteralValueAST      a literal
///   - IdentifierValueAST   a bare identifier
///   - FieldAccessValueAST  IDENTIFIER '.' IDENTIFIER
///   - InlineNodeValueAST   a node_expr used as a value
///
/// Each derives from BaseAST directly. There is no ValueAST family base:
/// the four forms share no fields, and the parser dispatches on the
/// concrete kind with isa<T>().
///
/// ─── NodeExprAST is shared ────────────────────────────────────────────────
/// A `node_expr` appears in two positions: as the right-hand side of a
/// node declaration, and as a value (via InlineNodeValueAST). NodeExprAST
/// is the shape itself; both positions hold a pointer to one.
///
/// ─── Literal content ──────────────────────────────────────────────────────
/// LiteralValueAST stores the literal's content, not its raw source text:
///   - For a string, the content with the surrounding quotes removed and
///     escape sequences resolved. The formatter re-escapes on output.
///   - For a char, the single resolved character. The formatter re-quotes
///     and re-escapes.
///   - For a number, the raw lexeme, including an optional leading sign,
///     e.g. "0xFF", "42", "-7", "+3.14", "-1.5e9". The sign is part of
///     the literal token, not a separate operator; the grammar has no
///     unary-minus production.
///   - For true/false/nil, the spelling ("true", "false", "nil").
///
/// Re-escaping is exact because the grammar's ESCAPE production lists
/// exactly seven escapes and nothing else. The formatter cannot reproduce
/// a source's choice of `\x41` over `A` because `\x41` does not exist in
/// this grammar.
///
/// ─── Dot and '::' do not mix ──────────────────────────────────────────────
/// A field access uses '.'. A qualified type or node type uses '::'. A
/// value never carries '::', because a value reaches a declaration by its
/// bare imported name. `Key.W` is an enum member; `core::Key` is a
/// qualified type; `core::Key.W` is not a value and not a type reference.

#pragma once

#include "core/ast/BaseAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/Tokens.hpp" // LiteralKind
#include "core/memory/InternedString.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// NodeExprAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief A node expression: `NodeType(args...)`.
///
/// The node type is stored as a TypeIdAST, matching `NodeType ::=
/// [ IDENTIFIER '::' ] IDENTIFIER`. The argument list is a span of value
/// nodes.
///
/// NodeExprAST is not itself a value. It is the shape a `node_expr` has;
/// the value form that uses it is InlineNodeValueAST, and the declaration
/// that uses it is NodeDeclAST.
struct NodeExprAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::NodeExpr;

    /// The node type name, stored as a TypeIdAST. Non-null after a
    /// successful parse. On a parse error, this may be null and
    /// `hasSyntaxError` is true.
    TypeIdAST *type = nullptr;

    /// The argument list. Each element is one of the four value nodes.
    /// The span is empty for a no-argument node expression.
    ArenaSpan<BaseAST *> args;

    NodeExprAST() : BaseAST(ASTKind::NodeExpr) {}

    NodeExprAST(TypeIdAST *t, ArenaSpan<BaseAST *> a)
        : BaseAST(ASTKind::NodeExpr), type(t), args(a) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// LiteralValueAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief A literal value.
///
/// A literal is one of the six forms the grammar's §1.4 lists: integer,
/// float, string, char, bool, or nil. The `kind` field says which; the
/// `text` field holds the content.
///
/// For string and char literals, `text` is the *resolved* content: the
/// surrounding delimiters removed and escape sequences applied. The
/// formatter re-quotes and re-escapes on output.
struct LiteralValueAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::LiteralValue;

    /// Which of the six literal forms this is.
    LiteralKind kind = LiteralKind::Int;

    /// The literal's content.
    ///
    ///   - Int / Float:  the raw lexeme, including an optional leading
    ///                   sign, e.g. "0xFF", "42", "-7", "+3.14",
    ///                   "-1.5e9". The sign is part of the token.
    ///   - String:       the resolved content, escapes applied, no quotes.
    ///   - Char:         the resolved single character, no quotes.
    ///   - Bool:         "true" or "false".
    ///   - Nil:          "nil".
    InternedString text;

    LiteralValueAST() : BaseAST(ASTKind::LiteralValue) {}

    LiteralValueAST(LiteralKind k, InternedString t)
        : BaseAST(ASTKind::LiteralValue), kind(k), text(t) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// IdentifierValueAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief A bare identifier used as a value.
///
///   `player`       -- a node, resource
///   `max_hp`       -- a resource field by name
///
/// The identifier is a single name; the parser does not resolve it.
/// Sema checks the name against the enclosing scopes.
struct IdentifierValueAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::IdentifierValue;

    InternedString name;

    IdentifierValueAST() : BaseAST(ASTKind::IdentifierValue) {}

    explicit IdentifierValueAST(InternedString n)
        : BaseAST(ASTKind::IdentifierValue), name(n) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// FieldAccessValueAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief A field access used as a value.
///
///   `Config.speed`          -- a resource field
///   `Key.A`                 -- an enum member
///   `player_health.current` -- a node output
///
/// The grammar's `value` production allows exactly one level of field
/// access: `IDENTIFIER '.' IDENTIFIER`. There is no `a.b.c`. A field
/// access uses '.', never '::'; a module-qualified name is a type
/// reference, not a value.
///
/// If a future grammar allows deeper field access, this node's two fields
/// become a span. Until then, the two named fields are the whole shape,
/// and a caller can read `object` and `field` without indexing a span.
struct FieldAccessValueAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::FieldAccessValue;

    /// The part before the '.'.
    InternedString object;

    /// The part after the '.'.
    InternedString field;

    FieldAccessValueAST() : BaseAST(ASTKind::FieldAccessValue) {}

    FieldAccessValueAST(InternedString o, InternedString f)
        : BaseAST(ASTKind::FieldAccessValue), object(o), field(f) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// InlineNodeValueAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief A node expression used as a value.
///
///   `Float32Node(200.0)` used as a node argument
///   `AddNode(a, b)` used as a resource default
///
/// The grammar says (§2.7): "A `node_expr` at value position creates an
/// inline node. It is valid but discouraged; a named node is more
/// readable and reusable." The AST accepts it; only Sema can enforce
/// any policy about it, and Sema does not do so by default.
struct InlineNodeValueAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::InlineNodeValue;

    /// The node expression. Non-null after a successful parse.
    NodeExprAST *node = nullptr;

    InlineNodeValueAST() : BaseAST(ASTKind::InlineNodeValue) {}

    explicit InlineNodeValueAST(NodeExprAST *n)
        : BaseAST(ASTKind::InlineNodeValue), node(n) {}
};