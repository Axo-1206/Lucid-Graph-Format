/// @file core/trivia/Trivia.hpp
///
/// @brief One piece of trivia: a comment in the source.
///
/// ─── What trivia is ───────────────────────────────────────────────────────
/// A comment. The lexer collects comments into a TriviaBuffer; the parser
/// ignores them; the formatter reads the buffer and emits them in their
/// original positions.
///
/// ─── The two kinds ────────────────────────────────────────────────────────
/// The grammar has two comment forms:
///
///     -- ...           line comment; runs to end of line
///     /- ... -/        block comment; nestable
///
/// Line comments and block comments are both trivia. They differ in how
/// they are emitted:
///
///   - A line comment's text is the content between `--` and the
///     newline (or end of file). The newline is not part of the text.
///   - A block comment's text is the content between `/-` and `-/`,
///     with the delimiters removed. Internal newlines are preserved.
///
/// The text is preserved exactly as written. The formatter does not trim
/// whitespace or reflow the comment.
///
/// ─── Interning ────────────────────────────────────────────────────────────
/// The text is an InternedString, like every other string in the codebase.
/// The lexer interns each comment's content once and stores the handle.
///
/// ─── No AST attachment ────────────────────────────────────────────────────
/// Trivia is not attached to tokens or to AST nodes. It lives in a flat,
/// sorted list. The formatter reads it while walking the AST; the LSP
/// (future) reads it while answering hover queries. No job mutates the
/// AST to carry comments.

#pragma once

#include "core/SourceLocation.hpp"
#include "core/memory/InternedString.hpp"

#include <cstdint>

namespace lucid::trivia
{

    /// @brief The kind of a trivia entry.
    enum class TriviaKind : uint8_t
    {
        LineComment,  // -- ...
        BlockComment, // /- ... -/
    };

    /// @brief One comment in the source.
    struct Trivia
    {
        /// Which comment form this is.
        TriviaKind kind = TriviaKind::LineComment;

        /// The comment's content, exactly as written, without the
        /// delimiters. For a line comment, this is the text between
        /// `--` and the newline. For a block comment, this is the text
        /// between `/-` and `-/`, with internal newlines preserved.
        ///
        /// The text is not trimmed. Leading and trailing whitespace
        /// inside the delimiters is preserved.
        InternedString text;

        /// The location of the comment's first character. For a line
        /// comment, this is the location of the first `-`. For a block
        /// comment, this is the location of the `/`.
        SourceLocation loc;
    };

} // namespace lucid::trivia