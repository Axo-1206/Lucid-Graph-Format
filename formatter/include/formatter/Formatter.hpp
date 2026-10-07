/// @file formatter/Formatter.hpp
///
/// @brief The formatter's public API.
///
/// ─── What the formatter does ──────────────────────────────────────────────
/// It takes a parsed ModuleAST and produces canonical Lucid source text.
/// "Canonical" means one layout per construct: indentation, spacing, and
/// line breaks are determined by the formatter, not by the input. The
/// input's layout is discarded. The AST's content is preserved exactly.
///
/// ─── Comments are preserved ───────────────────────────────────────────────
/// Comments are not part of the AST. They are collected by the lexer
/// into a TriviaBuffer and emitted by the formatter in their original
/// positions. The `format` entry point lexes the source to obtain the
/// trivia; the `formatModule` entry point lexes the source itself for
/// the same reason.
///
/// ─── Two entry points ─────────────────────────────────────────────────────
/// `format` takes source text and returns formatted text. It is the
/// public API and matches the specification in Architecture.md §9.
///
/// `formatModule` takes a parsed ModuleAST and the source text and
/// returns formatted text. It is the same operation, minus the parse.
/// It is used by callers that already have an AST — the formatter's own
/// tests, the round-trip fixture tests, an editor's format-on-save.
///
/// ─── What the formatter is not ────────────────────────────────────────────
/// It is not a source-preserving editor. It produces canonical output,
/// not minimal edits. A caller that wants to preserve the user's exact
/// formatting should not run the formatter.
///
/// It is not a pretty-printer with options. The two options in
/// FormatOptions are indent width and tabs-vs-spaces. There are no
/// options for bracket style, line breaking, or quote style.
///
/// ─── Idempotence ──────────────────────────────────────────────────────────
/// Formatting a formatted file produces the same file. This is a hard
/// requirement and is tested by the round-trip fixture tests. If
/// `format(format(x)) != format(x)`, there is a bug.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/StringPool.hpp"
#include "formatter/FormatOptions.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace lucid::formatter
{

    // ─── FormatResult ─────────────────────────────────────────────────────────

    /// @brief The result of the `format` entry point.
    struct FormatResult
    {
        /// True if the source parsed and the formatter produced output.
        /// False if the source had parse errors or the formatter failed.
        bool ok = false;

        /// The formatted text. Empty if `ok` is false.
        std::string text;

        /// The diagnostics from parsing, and any from formatting. Empty
        /// when the input was clean.
        std::vector<lucid::diag::Diagnostic> diagnostics;
    };

    // ─── The public entry point ───────────────────────────────────────────────

    /// @brief Format a source file into canonical Lucid text.
    ///
    /// Lexes and parses the source, then formats the AST. Comments in
    /// the source are preserved.
    ///
    /// If the source has parse errors, the function returns a result with
    /// `ok = false`, `text` empty, and `diagnostics` containing the
    /// parser's diagnostics. The formatter does not attempt to format a
    /// broken AST.
    ///
    /// @param source    The source text.
    /// @param filename  The file's name, for diagnostics.
    /// @param options   The formatter's options.
    /// @return The result.
    FormatResult format(std::string_view source,
                        std::string_view filename,
                        FormatOptions options = {});

    // ─── The lower-level entry point ──────────────────────────────────────────

    /// @brief Format a parsed module into canonical Lucid text.
    ///
    /// The module must have been produced by parsing `source`. The
    /// function re-lexes `source` to collect comments and walks the
    /// module's AST, interleaving the two. It does not re-parse.
    ///
    /// A module with syntax-error nodes is still formatted; the
    /// `UnknownAST` placeholder is emitted as an empty line, and the
    /// comments around it are preserved. The caller decides whether to
    /// show the result.
    ///
    /// @param module   The parsed module. May be null; a null module
    ///                 produces an empty string.
    /// @param source   The source text the module was parsed from.
    ///                 Used only to collect comments.
    /// @param pool     The StringPool that interned the module's
    ///                 strings and the source's comments.
    /// @param options  The formatter's options.
    /// @return The formatted text.
    std::string formatModule(const ModuleAST *module,
                             std::string_view source,
                             const StringPool &pool,
                             FormatOptions options = {});

} // namespace lucid::formatter