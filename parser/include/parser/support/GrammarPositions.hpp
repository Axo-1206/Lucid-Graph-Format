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
/// it can begin a `node_decl` and a `composite_body_decl` but not an
/// `import_decl` or a `value` (a grammar-position fact).
///
/// The predicates here are the grammar's start sets. They are used by:
///
///   - The parser's dispatch: `parseDecl` decides which declaration
///     parser to call by asking which start set the current token is in.
///
///   - The parser's loops: `parseFile`'s top-level loop and
///     `parseCompositeDecl`'s body loop both call the matching predicate
///     to decide whether to keep parsing or to stop.
///
///   - The recovery scans: after a failed parse, the caller builds a stop
///     set out of these predicates so the scanner lands on a token that
///     can begin the next construct.
///
/// ─── What this file does not answer ───────────────────────────────────────
/// It does not answer "is this token *legal* here?" That is the parser's
/// job per call site, and Sema's job for semantic rules. A predicate here
/// is a *set membership* test, not a validation.
///
/// It does not answer "should the parser consume this token?" A start-set
/// test says the token *can* begin a production; whether the parser is
/// in a position to parse that production is the caller's decision.
///
/// ─── Context-free, by design ──────────────────────────────────────────────
/// Every predicate takes a `TokenType` and nothing else. No previous
/// token, no brace depth, no parser state. A predicate that needed any
/// of those would not be a grammar-position fact; it would be a
/// parser-local decision, and it belongs at the call site, not here.
///
/// ─── The four predicates are a starting set ───────────────────────────────
/// Four productions currently need a start-set test:
///
///   - `top_decl`                 → canStartTopDecl
///   - `composite_body_decl`      → canStartCompositeBodyDecl
///   - `value`                    → canStartValue
///   - `type_id`                  → canStartType
///
/// A fifth predicate is added when a `rules/` file needs one and the
/// grammar position is genuinely context-free. Candidates currently under
/// consideration: `canStartTrigger` (for `trigger_list` recovery) and
/// `canStartResourceField` (for `resource` body recovery). Neither is
/// needed until its call site exists; adding one before then would be a
/// guess about how the recovery path will look.

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
    ///                | composite_decl
    ///
    /// and each of `resource_decl`, `node_decl`, and `composite_decl` may be
    /// preceded by an `attribute_list`, whose first token is `@`.
    ///
    /// The start set is therefore:
    ///
    ///   @            an attribute_list (before a resource / node / composite)
    ///   import       import_decl
    ///   enum         enum_decl
    ///   resource     resource_decl
    ///   node         node_decl
    ///   composite    composite_decl
    ///
    /// `KW_FROM` is reserved and begins no production; it is not in the set.
    /// `KW_ON`, `KW_INPUT`, `KW_OUTPUT` are composite-body keywords and do
    /// not begin a top-level declaration. `IDENTIFIER` is not in the set:
    /// no top-level production begins with a bare identifier.
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
        case TokenType::KW_COMPOSITE:
            return true;
        default:
            return false;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // canStartCompositeBodyDecl — the start set of `composite_body_decl`
    // ─────────────────────────────────────────────────────────────────────────────

    /// @brief True if `t` can begin a declaration inside a composite body.
    ///
    /// The grammar's `composite_body_decl` is:
    ///
    ///     composite_body_decl ::= import_decl
    ///                           | enum_decl
    ///                           | resource_decl
    ///                           | node_decl
    ///
    /// A composite body may **not** contain another composite: §2.7 of the
    /// grammar states "A composite may not be declared inside another
    /// composite." `KW_COMPOSITE` is therefore *not* in the start set. A
    /// `composite` keyword inside a body is a syntax error, and the parser
    /// reports it rather than silently accepting it.
    ///
    /// `AT_SIGN` **is** in the set, deliberately. The literal
    /// `composite_body_decl` production does not list `attribute_list`, but
    /// grammar §2.3 says an attribute list "may precede any of enum,
    /// resource, node, or composite", and §8.1 flags the placement of
    /// attributes on `node` and `enum` as an open question. To keep the
    /// parser forward-compatible with the likely resolution of §8.1, the
    /// composite-body loop accepts `@` and hands it to the attribute parser.
    /// Sema can reject an attribute that turns out to be illegal in this
    /// position, cheaply. See §8.1 of the grammar for the open question.
    ///
    /// `KW_INPUT` and `KW_OUTPUT` are **not** in the set. They begin the
    /// input and output blocks, which the composite parser consumes before
    /// the body loop starts. They never appear as the first token of a body
    /// declaration; if they do, the body loop's caller has already mis-parsed
    /// the composite's structure.
    inline bool canStartCompositeBodyDecl(TokenType t) noexcept
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
    /// The start set is therefore:
    ///
    ///   any literal   the literal form
    ///   IDENTIFIER    the identifier, field-access, or inline-node form
    ///
    /// Nothing else. `LPAREN` is not in the set: the grammar has no
    /// parenthesized-value production, and §4.5 confirms there is no
    /// expression grammar. No keyword is in the set: keywords are not
    /// identifiers, and the `value` production does not list one.
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
    ///     type_id ::= IDENTIFIER [ '.' IDENTIFIER ]
    ///
    /// The start set is exactly `IDENTIFIER`. Nothing else. This predicate
    /// is one line; it exists so that every call site that needs the test
    /// names the production rather than the token, and so that a future
    /// grammar change that admits another start token (a primitive keyword,
    /// say) touches one place.
    inline bool canStartType(TokenType t) noexcept
    {
        return t == TokenType::IDENTIFIER;
    }

} // namespace lucid::parser