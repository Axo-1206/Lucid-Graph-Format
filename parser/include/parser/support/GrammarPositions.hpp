/// @file parser/support/GrammarPositions.hpp
///
/// @brief The parser's start-set predicates: "can this token begin
///        production X?"
///
/// ─── What this file answers ───────────────────────────────────────────────
/// `Tokens.hpp` answers "what kind of token is this?" — keyword, literal,
/// punctuation, delimiter. This file answers a different question: "can
/// this token be the *first* token of this grammar production?" The two
/// are not the same. `KW_NODE` is a declaration keyword (a token fact);
/// it can begin a `node_decl` but not an `import_decl` or a `value` (a
/// grammar-position fact).
///
/// ─── What this file does not answer ───────────────────────────────────────
/// It does not answer "is this token *legal* here?" That is the parser's
/// job per call site, and Sema's job for semantic rules. A predicate here
/// is a *set membership* test, not a validation.
///
/// ─── Context-free, by design ──────────────────────────────────────────────
/// Every predicate takes a `TokenType` and nothing else. No previous
/// token, no brace depth, no parser state.
///
/// ─── The three predicates ─────────────────────────────────────────────────
/// Three productions need a start-set test:
///
///   - `top_decl`                 → canStartTopDecl
///   - `value`                    → canStartValue
///   - `type_id`                  → canStartType

#pragma once

#include "core/Tokens.hpp"

namespace lucid::parser
{

    // ─────────────────────────────────────────────────────────────────────────────
    // canStartTopDecl — the start set of `top_decl`
    // ─────────────────────────────────────────────────────────────────────────────

    /// @brief True if `t` can begin a top-level declaration.
    ///
    /// The grammar's `top_decl` is:
    ///
    ///     top_decl ::= import_decl
    ///                | enum_decl
    ///                | resource_decl
    ///                | node_decl
    ///
    /// and each of the four declarations may be preceded by an
    /// `attribute_list`, whose first token is `@`.
    ///
    /// The start set is therefore:
    ///
    ///   @            an attribute_list (before any of the four)
    ///   import       import_decl
    ///   enum         enum_decl
    ///   resource     resource_decl
    ///   node         node_decl
    ///
    /// `KW_FROM` is reserved and begins no production; it is not in the set.
    /// `KW_ON` is a node-body keyword and does not begin a top-level
    /// declaration. `IDENTIFIER` is not in the set: no top-level production
    /// begins with a bare identifier.
    ///
    /// `EOF_TOKEN` is not in the set. The caller checks `stream.isAtEnd()`
    /// separately; a start-set test is never asked about EOF.
    inline bool canStartTopDecl(TokenType t) noexcept
    {
        switch (t)
        {
        case TokenType::AT_SIGN:
        case TokenType::KW_IMPORT:
        case TokenType::KW_ENUM:
        case TokenType::KW_RESOURCE:
        case TokenType::KW_NODE:
            return true;
        default:
            return false;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // canStartValue — the start set of `value`
    // ─────────────────────────────────────────────────────────────────────────────

    /// @brief True if `t` can begin a value.
    ///
    /// The grammar's `value` is:
    ///
    ///     value ::= literal
    ///             | IDENTIFIER
    ///             | IDENTIFIER '.' IDENTIFIER
    ///             | node_expr
    ///
    /// The first three forms begin with a literal token or an identifier.
    /// The fourth form (`node_expr`) also begins with an identifier — the
    /// node type name — and is distinguished from the bare-identifier and
    /// field-access forms by what follows.
    ///
    /// A signed literal (`-7`, `+3.14`) is a single literal token; the
    /// `-` or `+` is part of the literal and does not appear here as a
    /// separate token type. A value never begins with `-` or `+` as a
    /// token; it begins with a literal token whose text happens to carry
    /// a sign.
    inline bool canStartValue(TokenType t) noexcept
    {
        if (t == TokenType::IDENTIFIER)
        {
            return true;
        }
        return isLiteral(t);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // canStartType — the start set of `type_id`
    // ─────────────────────────────────────────────────────────────────────────────

    /// @brief True if `t` can begin a type reference.
    ///
    /// The grammar's `type_id` is:
    ///
    ///     type_id ::= [ IDENTIFIER '::' ] IDENTIFIER
    ///
    /// The start set is exactly `IDENTIFIER`. The optional qualifier is
    /// read by the caller (`parseTypeId`), which then checks for `::` and
    /// a second identifier. This predicate answers only "can the type
    /// reference start here?", and it can only start with an identifier.
    ///
    /// The `::` separator is a distinct token from `:`; it does not appear
    /// in the start set, and it does not appear in a `type_id` in the
    /// resource-field separator position.
    inline bool canStartType(TokenType t) noexcept
    {
        return t == TokenType::IDENTIFIER;
    }

} // namespace lucid::parser