/// @file parser/support/ErrorRecovery.hpp
///
/// @brief Synchronization utilities for the Lucid parser.
///
/// ─── The recovery model ───────────────────────────────────────────────────
/// Every parser function that can fail has one of two behaviors on error:
///
///   1. PARTIAL-PARSE. It reports a diagnostic, produces an AST node with
///      `hasSyntaxError = true`, and returns that node. Sema skips a node
///      that carries the flag. This is what the LSP wants: as much
///      structure as possible, marked.
///
///   2. SKIP. It reports a diagnostic and returns `nullptr`. There is no
///      honest node to produce. The caller that received the `nullptr`
///      runs a synchronizer to skip to the next safe point and continues.
///
/// This file provides the synchronizers used by the second case. It does
/// not implement the partial-parse behavior of any specific parser
/// function; that is each parser's own decision, made at its own error
/// sites.
///
/// ─── Two recovery contexts, no more ───────────────────────────────────────
/// The Lucid Graph Format has exactly two nested declaration lists:
///
///   - The top level: `program ::= { top_decl }`. The recovery context is
///     "a top-level declaration failed; skip to the next one."
///
///   - A composite body: `composite_decl`'s body is a sequence of
///     `composite_body_decl`. The recovery context is "a body declaration
///     failed; skip to the next one, or to the composite's closing `}`."
///
/// There are no statements, no function bodies, no block-local
/// declarations, and no other nested construct that needs its own recovery
/// scan. The two contexts above are the entire set.
///
/// This file does not define the stop sets for those two contexts. A stop
/// set is policy: it depends on which construct the caller is recovering
/// into, and on what tokens are legal at that construct's start. Each stop
/// set lives next to its single caller — one in `Parser.cpp`, one in
/// `ParseComposite.cpp` — and is built from the start-set predicates in
/// `GrammarPositions.hpp`. This file provides only the mechanism.
///
/// ─── Design: two scans, one primitive ─────────────────────────────────────
/// `synchronizeUntil` is the depth-blind scan: its predicate is
/// `bool(TokenType)` and is consulted only at bracket depth zero.
///
/// `synchronizeUntilDepth` is the depth-aware scan: its predicate is
/// `bool(TokenStream&, int)` and is consulted at every token. Use it when
/// the recovery decision depends on whether the scan is inside a lost
/// block, or when the predicate needs to look ahead (`FN` followed by an
/// identifier is a strong declaration start; a bare `FN` is not).
///
/// `synchronizeTo` is a variadic convenience over `synchronizeUntil`.
///
/// ─── Design: no ParserContext parameter ───────────────────────────────────
/// A synchronizer is called precisely because the caller already reported
/// a diagnostic; a second diagnostic on the same broken construct is
/// noise. The scanner therefore does not take a `ParserContext&` and does
/// not report diagnostics of its own. It consumes tokens and returns why
/// it stopped.
///
/// ─── Design: bracket-aware scanning ───────────────────────────────────────
/// The scan tracks a local stack of open `(`, `[`, `{`. A stop token is
/// only honored at bracket depth zero for `synchronizeUntil`, and at any
/// depth for `synchronizeUntilDepth` (the caller's predicate decides).
/// This is what makes `a(foo) bar` scan past the `foo` inside the
/// parentheses rather than stopping at it, even if `foo` is in the stop
/// set.
///
/// ─── Design: three ways to stop ───────────────────────────────────────────
///   - `Matched`        — the predicate matched at depth zero (or at any
///                        depth, for `synchronizeUntilDepth`). The scan
///                        stops *on* the token; it does not consume it.
///   - `ForeignCloser`  — a closer with no matching opener on the scan's
///                        own stack. The caller should recover upward.
///                        In the new grammar the only foreign closer a
///                        top-level scan can meet is a stray `}` from a
///                        missing `{`; it is a real case, not a corner.
///   - `ReachedEnd`     — end of input.
///
/// ─── Design: the caller owns the stop set ─────────────────────────────────
/// A synchronizer has no built-in rule about what constitutes a safe
/// resume point. The caller passes a stop set that reflects the construct
/// it is recovering into. The start-set predicates the caller builds that
/// stop set from live in `GrammarPositions.hpp`; this file does not
/// include that header, because the scanner itself does not need it.

#pragma once

#include "core/Tokens.hpp"
#include "parser/context/TokenStream.hpp"

#include <vector>

namespace lucid::parser
{

    // =============================================================================
    // SyncResult
    // =============================================================================

    /// @brief Why a synchronization call stopped.
    enum class SyncResult
    {
        /// The predicate matched a token. For `synchronizeUntil` the match is
        /// at bracket depth zero; for `synchronizeUntilDepth` the predicate
        /// decides whether depth matters. The current token is one of the
        /// caller's own targets; the caller may act on it directly.
        Matched,

        /// The scan encountered a closing bracket that does not match any
        /// opener on its own stack. The bracket belongs to an enclosing
        /// construct. The caller should treat this as "no target found in
        /// this construct" and recover upward.
        ///
        /// In the new grammar, the only foreign closer a top-level scan can
        /// meet is a stray `}` from a missing `{` earlier in the file. A
        /// composite-body scan can meet the composite's own closing `}`,
        /// which it treats as a foreign closer unless its stop set already
        /// stopped there.
        ForeignCloser,

        /// The scan reached end-of-input.
        ReachedEnd,
    };

    // =============================================================================
    // Internal helpers
    // =============================================================================

    namespace detail
    {

        /// @brief The closing bracket that matches an opening bracket.
        ///
        /// Precondition: `opener` is `LPAREN`, `LBRACKET`, or `LBRACE`. The
        /// default case is unreachable for a caller that honors the
        /// precondition; it returns `RPAREN` so the function is total.
        inline TokenType matchingCloserFor(TokenType opener) noexcept
        {
            switch (opener)
            {
            case TokenType::LPAREN:
                return TokenType::RPAREN;
            case TokenType::LBRACKET:
                return TokenType::RBRACKET;
            case TokenType::LBRACE:
                return TokenType::RBRACE;
            default:
                return TokenType::RPAREN;
            }
        }

    } // namespace detail

    // =============================================================================
    // synchronizeUntil — the depth-blind scanning primitive
    // =============================================================================

    /// @brief Skip tokens until `stopAt` matches at bracket depth zero, or
    ///        until a foreign closer or end-of-input ends the scan.
    ///
    /// The scan tracks the bracket kinds `(`, `[`, `{` and their closers. An
    /// opener is pushed onto a local stack; a closer pops the stack if it
    /// matches the top, or triggers `SyncResult::ForeignCloser` if it does
    /// not. A closer that matches a top-of-stack opener consumes both.
    ///
    /// The predicate is consulted only at bracket depth zero. A token inside
    /// a lost `(...)`, `[...]`, or `{...}` never stops the scan, no matter
    /// what the predicate says.
    ///
    /// The scan stops *on* a matched token; it does not consume it. The
    /// caller sees the matched token as the stream's current token.
    ///
    /// Use this form when the recovery decision does not depend on how deep
    /// the scan is — a "skip to the next comma or close-paren" recovery, or
    /// a "skip to the next keyword in a fixed set" recovery.
    ///
    /// @tparam Predicate  A callable `bool(TokenType)`. Called at depth-zero
    ///                    tokens only.
    /// @param stream      The token stream to scan.
    /// @param stopAt      The predicate that determines when to stop.
    /// @return Why the scan stopped.
    template <typename Predicate>
    SyncResult synchronizeUntil(TokenStream &stream, Predicate stopAt)
    {
        std::vector<TokenType> expectedClosers;

        while (!stream.isAtEnd())
        {
            const TokenType current = stream.peekType();

            // ─── Closer ───────────────────────────────────────────────────────
            // A closer that matches the top of the stack pops it and is
            // consumed. A closer with an empty stack, or one that does not
            // match the top, ends the scan: it belongs to an enclosing
            // construct that this scan is not inside of.
            if (isClosingDelimiter(current))
            {
                if (!expectedClosers.empty() &&
                    expectedClosers.back() == current)
                {
                    expectedClosers.pop_back();
                    stream.consume();
                    continue;
                }
                if (expectedClosers.empty() && stopAt(current))
                {
                    return SyncResult::Matched;
                }
                return SyncResult::ForeignCloser;
            }

            // ─── Depth-zero predicate check ───────────────────────────────────
            if (expectedClosers.empty() && stopAt(current))
            {
                return SyncResult::Matched;
            }

            // ─── Opener ───────────────────────────────────────────────────────
            if (isOpeningDelimiter(current))
            {
                expectedClosers.push_back(detail::matchingCloserFor(current));
            }
            stream.consume();
        }

        return SyncResult::ReachedEnd;
    }

    // =============================================================================
    // synchronizeUntilDepth — the depth-aware scanning primitive
    // =============================================================================

    /// @brief Skip tokens until `stopAt` matches, passing the current
    ///        bracket depth and the stream to the predicate.
    ///
    /// Identical scanning to `synchronizeUntil`, with two differences:
    ///
    ///   - The predicate is `bool(TokenStream&, int depth)`. The stream is
    ///     passed so the predicate can look ahead (to distinguish a keyword
    ///     followed by an identifier from a bare keyword, for instance); the
    ///     depth is passed so the predicate can distinguish "a declaration
    ///     start at the top level" from "a declaration start inside a lost
    ///     block".
    ///
    ///   - The predicate is consulted at every token, not only at depth
    ///     zero. A caller that wants depth-aware behavior is by definition
    ///     interested in tokens at depth > 0.
    ///
    /// The scan still stops *on* a matched token, without consuming it.
    /// `ForeignCloser` and `ReachedEnd` have the same meanings as in the
    /// single-argument form.
    ///
    /// Use this form when the recovery decision depends on context — the
    /// top-level and composite-body recovery scans are the two current users,
    /// and both need to know whether they are inside a lost `{...}`.
    ///
    /// @tparam Predicate  A callable `bool(TokenStream&, int)`.
    /// @param stream      The token stream to scan.
    /// @param stopAt      The predicate.
    /// @return Why the scan stopped.
    template <typename Predicate>
    SyncResult synchronizeUntilDepth(TokenStream &stream, Predicate stopAt)
    {
        std::vector<TokenType> expectedClosers;

        while (!stream.isAtEnd())
        {
            const TokenType current = stream.peekType();
            const int depth = static_cast<int>(expectedClosers.size());

            // ─── Closer ───────────────────────────────────────────────────────
            if (isClosingDelimiter(current))
            {
                if (!expectedClosers.empty() &&
                    expectedClosers.back() == current)
                {
                    expectedClosers.pop_back();
                    stream.consume();
                    continue;
                }
                // A foreign closer: the predicate still gets a chance to stop
                // here, because a caller may want to treat "the enclosing
                // construct's `}`" as a legitimate stop point. The
                // composite-body scan does exactly this.
                if (stopAt(stream, depth))
                {
                    return SyncResult::Matched;
                }
                return SyncResult::ForeignCloser;
            }

            // ─── Predicate check at every depth ───────────────────────────────
            if (stopAt(stream, depth))
            {
                return SyncResult::Matched;
            }

            // ─── Opener ───────────────────────────────────────────────────────
            if (isOpeningDelimiter(current))
            {
                expectedClosers.push_back(detail::matchingCloserFor(current));
            }
            stream.consume();
        }

        return SyncResult::ReachedEnd;
    }

    // =============================================================================
    // synchronizeTo — the variadic convenience form
    // =============================================================================

    /// @brief Skip tokens until the current token matches any of the given
    ///        types at bracket depth zero.
    ///
    /// A thin wrapper over `synchronizeUntil` that turns the token-type pack
    /// into a predicate. Useful for the "skip to one of these punctuation
    /// tokens" recovery that appears in `parseArgList` and `parseTriggerList`.
    ///
    ///     // Skip to the next comma or close-paren.
    ///     synchronizeTo(stream, TokenType::COMMA, TokenType::RPAREN);
    ///
    /// For recovery into a declaration list, use `synchronizeUntilDepth` with
    /// a predicate built from `GrammarPositions.hpp`; the depth matters there
    /// and this convenience form does not carry it.
    template <typename... StopTokens>
    SyncResult synchronizeTo(TokenStream &stream, StopTokens... stopTokens)
    {
        return synchronizeUntil(stream, [stopTokens...](TokenType t)
                                { return ((t == stopTokens) || ...); });
    }

} // namespace lucid::parser