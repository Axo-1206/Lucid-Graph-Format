/**
 * @file core/diagnostics/Diagnostic.hpp
 * @brief The diagnostic engine: collection, query, and formatting.
 *
 * ─── Design: engine is session-scoped, not a singleton ────────────────────
 * A DiagnosticEngine is created by whoever runs a compilation — the CLI,
 * the LSP server, a test — and passed by reference to every stage that can
 * report. It is not global state.
 *
 * ─── Design: messages are built at the call site ──────────────────────────
 * There is no code-to-message table. A diagnostic's text is assembled from
 * the variadic arguments passed to `error`/`warning`/`note`/`hint`.
 *
 * ─── Design: no AST dependency in this header ─────────────────────────────
 * The AST-aware overloads (`error(code, BaseAST*, args...)`) are templates
 * that forward to a `.cpp` helper. This header does not include the AST
 * header tree; keeping it out is the difference between a fast incremental
 * build and a slow one.
 *
 * ─── Design: free-text notes and hints ────────────────────────────────────
 * A note or hint has no diagnostic identity of its own. It is reported
 * with `note`/`hint`, which take no DiagCode, and is stored with code 0.
 * The formatting code emits no code prefix for code 0.
 */

#pragma once

#include "core/SourceLocation.hpp"
#include "core/diagnostics/DiagCode.hpp"
#include "core/memory/InternedString.hpp"

#include <cstdint>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Forward declarations: the AST-aware overloads need the pointer type, not
// the definition. The actual `node->loc` dereference lives in a .cpp.
struct BaseAST;
class StringPool;

namespace lucid::diag
{

    // ─────────────────────────────────────────────────────────────────────────────
    // locationOf — declared here, defined in Diagnostic.cpp
    // ─────────────────────────────────────────────────────────────────────────────
    //
    // This declaration must appear BEFORE DiagnosticEngine. The AST-aware
    // overloads of error/warning/note/hint are templates. Inside a template, a
    // non-dependent name like `locationOf(node)` must be declared at the point
    // of the template's definition. If the declaration is below the class, the
    // compiler reports:
    //
    //   "there are no arguments to 'locationOf' that depend on a template
    //    parameter, so a declaration of 'locationOf' must be available"
    //
    // Moving the declaration up fixes it. The definition stays in
    // Diagnostic.cpp so this header does not include the AST header tree.

    SourceLocation locationOf(const BaseAST *node) noexcept;

    // ─────────────────────────────────────────────────────────────────────────────
    // Diagnostic
    // ─────────────────────────────────────────────────────────────────────────────

    /// @brief One collected diagnostic.
    ///
    /// A diagnostic is a value: severity, code, location, message, and the file
    /// it belongs to. Once added to the engine, it is immutable and can be
    /// copied, stored, or serialized.
    struct Diagnostic
    {
        Severity severity;
        DiagCode code; // code 0 for free-text notes and hints
        SourceLocation location;
        std::string message;
        InternedString file; // may be invalid

        DiagCategory category() const noexcept { return categoryFromCode(code); }
        bool isFreeText() const noexcept { return raw(code) == 0; }
        bool isError() const noexcept
        {
            return severity == Severity::Error || severity == Severity::Fatal;
        }
        bool isWarning() const noexcept { return severity == Severity::Warning; }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // DiagnosticEngine
    // ─────────────────────────────────────────────────────────────────────────────

    /// @brief Collects, queries, and formats diagnostics.
    ///
    /// The engine owns the diagnostics it has collected. It is not thread-safe:
    /// the pipeline is sequential. If a future stage wants to report from a
    /// worker thread, it queues the message and reports on the main thread.
    ///
    /// ─── String pool ──────────────────────────────────────────────────────────
    /// The engine holds an optional pointer to the session's StringPool, used
    /// only by `toString(InternedString)`. If the pointer is null, an interned
    /// string formats as `<interned:N>` rather than crashing — a diagnostic
    /// raised before the pool is set up still formats.
    ///
    /// ─── Current file ─────────────────────────────────────────────────────────
    /// The engine tracks the "current file" as a convenience for callers that
    /// report against a `SourceLocation` and do not want to pass the file
    /// explicitly. A driver sets it on entry to a file and restores the
    /// previous value on exit. A diagnostic added while no file is set has an
    /// invalid `file` field; callers should render that as "unknown file".
    class DiagnosticEngine
    {
    public:
        DiagnosticEngine() = default;
        explicit DiagnosticEngine(StringPool *pool) : m_pool(pool) {}

        StringPool *stringPool() const noexcept { return m_pool; }
        void setStringPool(StringPool *pool) noexcept { m_pool = pool; }

        InternedString currentFile() const noexcept { return m_currentFile; }
        void setCurrentFile(InternedString f) noexcept { m_currentFile = f; }

        // ─── Report: AST-anchored ───────────────────────────────────────────

        template <typename... Args>
        void error(DiagCode code, const BaseAST *node, Args &&...args)
        {
            add(severityFromCode(code), code,
                node ? locationOf(node) : SourceLocation{},
                buildMessage(std::forward<Args>(args)...));
        }

        template <typename... Args>
        void warning(DiagCode code, const BaseAST *node, Args &&...args)
        {
            add(Severity::Warning, code,
                node ? locationOf(node) : SourceLocation{},
                buildMessage(std::forward<Args>(args)...));
        }

        template <typename... Args>
        void note(const BaseAST *node, Args &&...args)
        {
            add(Severity::Note, DiagCode(0),
                node ? locationOf(node) : SourceLocation{},
                buildMessage(std::forward<Args>(args)...));
        }

        template <typename... Args>
        void hint(const BaseAST *node, Args &&...args)
        {
            add(Severity::Hint, DiagCode(0),
                node ? locationOf(node) : SourceLocation{},
                buildMessage(std::forward<Args>(args)...));
        }

        // ─── Report: location-anchored ──────────────────────────────────────

        template <typename... Args>
        void errorAt(DiagCode code, const SourceLocation &loc, Args &&...args)
        {
            add(severityFromCode(code), code, loc,
                buildMessage(std::forward<Args>(args)...));
        }

        template <typename... Args>
        void warningAt(DiagCode code, const SourceLocation &loc, Args &&...args)
        {
            add(Severity::Warning, code, loc,
                buildMessage(std::forward<Args>(args)...));
        }

        template <typename... Args>
        void noteAt(const SourceLocation &loc, Args &&...args)
        {
            add(Severity::Note, DiagCode(0), loc,
                buildMessage(std::forward<Args>(args)...));
        }

        template <typename... Args>
        void hintAt(const SourceLocation &loc, Args &&...args)
        {
            add(Severity::Hint, DiagCode(0), loc,
                buildMessage(std::forward<Args>(args)...));
        }

        template <typename... Args>
        void fatalAt(DiagCode code, const SourceLocation &loc, Args &&...args)
        {
            add(Severity::Fatal, code, loc,
                buildMessage(std::forward<Args>(args)...));
        }

        // ─── Query ──────────────────────────────────────────────────────────

        bool hasErrors() const noexcept;
        bool hasWarnings() const noexcept;
        int errorCount() const noexcept;
        int warningCount() const noexcept;

        int totalCount() const noexcept
        {
            return static_cast<int>(m_diagnostics.size());
        }

        bool canContinue(int maxErrors = 100) const noexcept
        {
            return errorCount() < maxErrors;
        }

        const std::vector<Diagnostic> &all() const noexcept { return m_diagnostics; }
        bool empty() const noexcept { return m_diagnostics.empty(); }
        void clear() noexcept { m_diagnostics.clear(); }

        // ─── Formatting ─────────────────────────────────────────────────────

        void dump(std::ostream &os) const;
        void dumpWithColor(std::ostream &os) const;

        std::string formatOneLine(const Diagnostic &d) const;
        std::string formatOneLineWithColor(const Diagnostic &d) const;

    private:
        std::vector<Diagnostic> m_diagnostics;
        InternedString m_currentFile;
        StringPool *m_pool = nullptr;

        void add(Severity sev, DiagCode code,
                 const SourceLocation &loc, std::string msg)
        {
            m_diagnostics.push_back(
                Diagnostic{sev, code, loc, std::move(msg), m_currentFile});
        }

        std::string toString(InternedString s) const;
        std::string toString(const char *s) const { return s ? std::string(s) : "null"; }
        std::string toString(std::string_view s) const { return std::string(s); }
        std::string toString(const std::string &s) const { return s; }
        std::string toString(bool b) const { return b ? "true" : "false"; }
        std::string toString(char c) const { return std::string(1, c); }

        std::string toString(int32_t v) const { return std::to_string(v); }
        std::string toString(uint32_t v) const { return std::to_string(v); }
        std::string toString(int64_t v) const { return std::to_string(v); }
        std::string toString(uint64_t v) const { return std::to_string(v); }
        std::string toString(double v) const { return std::to_string(v); }

        template <typename T>
        std::string toString(const T *ptr) const
        {
            if (!ptr)
                return "null";
            std::ostringstream oss;
            oss << ptr;
            return oss.str();
        }

        template <typename T>
        std::string toString(const T &value) const
        {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }

        template <typename First, typename... Rest>
        std::string buildMessage(First &&first, Rest &&...rest) const
        {
            return toString(std::forward<First>(first)) + buildMessage(std::forward<Rest>(rest)...);
        }

        std::string buildMessage() const { return {}; }
    };

} // namespace lucid::diag