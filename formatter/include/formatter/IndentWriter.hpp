/// @file formatter/IndentWriter.hpp
///
/// @brief A text emitter that tracks the current indent level and column.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A small helper the formatter uses to emit text. It owns an output
/// buffer, tracks the current indentation level and the current column,
/// and provides two operations:
///
///   - `write(text)`    — append text to the current line.
///   - `newline()`      — end the current line and start a new one.
///
/// The formatter calls these two operations everywhere. Nothing else
/// touches the output buffer or computes an indent by hand.
///
/// ─── When the indent is written ───────────────────────────────────────────
/// The indent is written **lazily**: on the next `write()` after a
/// `newline()`, not during the `newline()` itself. This is what makes
/// the following work:
///
///     w.write("hp: int");
///     w.newline();
///     w.dedent();
///     w.write("}");
///
/// The `newline()` does not write the indent. The `dedent()` lowers the
/// level. The `write("}")` writes the indent at the *current* level,
/// which is now zero. The result is `}` at column 0, which is what a
/// caller expects.
///
/// If the indent were written eagerly by `newline()`, the four spaces
/// would already be in the buffer when `dedent()` runs, and nothing
/// would remove them.
///
/// ─── Blank lines have no trailing whitespace ──────────────────────────────
/// If `newline()` is called twice in a row, the second call appends a
/// `\n` without writing the indent. The blank line is truly blank; it
/// has no trailing spaces. This is the "no trailing whitespace" rule
/// that most editors and formatters enforce.
///
/// ─── What this is not ─────────────────────────────────────────────────────
/// It is not a streaming writer to disk. It owns an in-memory buffer and
/// returns it via `str()`. For the formatter's use case — a string in,
/// a string out — this is sufficient.
///
/// It is not a line-breaking engine. There is no `writeWrapped()` and
/// no awareness of a maximum column. If a caller writes a long string,
/// the line is long.
///
/// ─── Column tracking ──────────────────────────────────────────────────────
/// `column()` returns the number of **bytes** written since the last
/// newline. It is not a count of codepoints or display cells. A UTF-8
/// glyph that takes three bytes advances the column by three. This is
/// adequate for ASCII output and documented as a limitation.
///
/// ─── Indentation ──────────────────────────────────────────────────────────
/// The writer holds a single indent level. `indent()` raises it,
/// `dedent()` lowers it. `dedent` past zero clamps at zero.
///
/// When `options.use_tabs` is true, the indent is one `\t` per level.
/// When false, the indent is `indent_width` spaces per level.

#pragma once

#include "formatter/FormatOptions.hpp"

#include <string>
#include <string_view>

namespace lucid::formatter
{

    class IndentWriter
    {
    public:
        // ─── Construction ──────────────────────────────────────────────────

        explicit IndentWriter(const FormatOptions &options = {})
            : m_options(options) {}

        IndentWriter(const IndentWriter &) = delete;
        IndentWriter &operator=(const IndentWriter &) = delete;
        IndentWriter(IndentWriter &&) = default;
        IndentWriter &operator=(IndentWriter &&) = default;

        // ─── Indent level ──────────────────────────────────────────────────

        void indent() { ++m_indentLevel; }

        void dedent()
        {
            if (m_indentLevel > 0)
            {
                --m_indentLevel;
            }
        }

        int indentLevel() const noexcept { return m_indentLevel; }

        // ─── Writing ───────────────────────────────────────────────────────

        /// Append text to the current line. If the previous operation was
        /// a `newline()` and the indent has not yet been written, this
        /// call writes the indent first (at the current level), then the
        /// text.
        void write(std::string_view text)
        {
            flushPendingIndent();
            m_buffer.append(text.data(), text.size());
            m_column += text.size();
        }

        /// Append a single character. Equivalent to `write({&c, 1})`.
        void write(char c)
        {
            flushPendingIndent();
            m_buffer += c;
            ++m_column;
        }

        /// End the current line and start a new one. The indent is not
        /// written here; it is written lazily by the next `write()`.
        /// If the previous call was already a `newline()` (a blank
        /// line), the indent is not written for the blank line either.
        void newline()
        {
            m_buffer += '\n';
            m_column = 0;
            m_pendingIndent = true;
        }

        // ─── Queries ───────────────────────────────────────────────────────

        /// The number of bytes on the current line, not counting the
        /// pending indent (which has not been written yet).
        size_t column() const noexcept { return m_column; }

        /// The full output so far.
        const std::string &str() const noexcept { return m_buffer; }

        /// The full output so far, moved out. After this call the writer
        /// is empty; callers that want to keep using the writer should
        /// not call this.
        std::string takeStr() { return std::move(m_buffer); }

        // ─── The options, for callers that need them ──────────────────────

        const FormatOptions &options() const noexcept { return m_options; }

    private:
        // ─── Pending indent ────────────────────────────────────────────────

        void flushPendingIndent()
        {
            if (!m_pendingIndent)
            {
                return;
            }
            writeIndent();
            m_column = static_cast<size_t>(indentWidthInBytes());
            m_pendingIndent = false;
        }

        int indentWidthInBytes() const noexcept
        {
            return m_options.use_tabs ? m_indentLevel
                                      : m_indentLevel * m_options.indent_width;
        }

        void writeIndent()
        {
            if (m_options.use_tabs)
            {
                for (int i = 0; i < m_indentLevel; ++i)
                {
                    m_buffer += '\t';
                }
            }
            else
            {
                const int count = m_indentLevel * m_options.indent_width;
                for (int i = 0; i < count; ++i)
                {
                    m_buffer += ' ';
                }
            }
        }

        // ─── State ─────────────────────────────────────────────────────────

        FormatOptions m_options;
        std::string m_buffer;
        int m_indentLevel = 0;
        size_t m_column = 0;
        bool m_pendingIndent = false;
    };

} // namespace lucid::formatter