/// @file core/diagnostics/Diagnostic.cpp
/// @brief Implementation of DiagnosticEngine.

#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/StringPool.hpp"

namespace lucid::diag
{

    // ─────────────────────────────────────────────────────────────────────────────
    // Query
    // ─────────────────────────────────────────────────────────────────────────────

    bool DiagnosticEngine::hasErrors() const noexcept
    {
        for (const auto &d : m_diagnostics)
        {
            if (d.isError())
                return true;
        }
        return false;
    }

    bool DiagnosticEngine::hasWarnings() const noexcept
    {
        for (const auto &d : m_diagnostics)
        {
            if (d.isWarning())
                return true;
        }
        return false;
    }

    int DiagnosticEngine::errorCount() const noexcept
    {
        int n = 0;
        for (const auto &d : m_diagnostics)
        {
            if (d.isError())
                ++n;
        }
        return n;
    }

    int DiagnosticEngine::warningCount() const noexcept
    {
        int n = 0;
        for (const auto &d : m_diagnostics)
        {
            if (d.isWarning())
                ++n;
        }
        return n;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Formatting
    // ─────────────────────────────────────────────────────────────────────────────

    namespace
    {

        constexpr const char *kReset = "\x1b[0m";
        constexpr const char *kRed = "\x1b[31m";
        constexpr const char *kYellow = "\x1b[33m";
        constexpr const char *kCyan = "\x1b[36m";
        constexpr const char *kGray = "\x1b[90m";

        const char *severityColor(Severity s)
        {
            switch (s)
            {
            case Severity::Error:
            case Severity::Fatal:
                return kRed;
            case Severity::Warning:
                return kYellow;
            case Severity::Note:
            case Severity::Hint:
                return kCyan;
            }
            return kReset;
        }

    } // namespace

    std::string DiagnosticEngine::formatOneLine(const Diagnostic &d) const
    {
        std::string out;
        out += '[';
        out += severityName(d.severity);
        out += "] ";

        if (!d.isFreeText())
        {
            out += 'E';
            out += std::to_string(raw(d.code));
            out += ": ";
        }

        out += d.message;

        if (d.location.isKnown())
        {
            out += " at ";
            out += std::to_string(d.location.line());
            out += ':';
            out += std::to_string(d.location.column());
        }

        return out;
    }

    std::string DiagnosticEngine::formatOneLineWithColor(const Diagnostic &d) const
    {
        std::string out;
        out += severityColor(d.severity);
        out += '[';
        out += severityName(d.severity);
        out += "] ";
        out += kReset;

        if (!d.isFreeText())
        {
            out += kGray;
            out += 'E';
            out += std::to_string(raw(d.code));
            out += ": ";
            out += kReset;
        }

        out += d.message;

        if (d.location.isKnown())
        {
            out += kGray;
            out += " at ";
            out += std::to_string(d.location.line());
            out += ':';
            out += std::to_string(d.location.column());
            out += kReset;
        }

        return out;
    }

    void DiagnosticEngine::dump(std::ostream &os) const
    {
        for (const auto &d : m_diagnostics)
        {
            os << formatOneLine(d) << '\n';
        }
        if (!m_diagnostics.empty())
        {
            const int e = errorCount();
            const int w = warningCount();
            os << "--- " << totalCount() << " diagnostic"
               << (totalCount() == 1 ? "" : "s")
               << " (" << e << " error" << (e == 1 ? "" : "s")
               << ", " << w << " warning" << (w == 1 ? "" : "s")
               << ")\n";
        }
    }

    void DiagnosticEngine::dumpWithColor(std::ostream &os) const
    {
        for (const auto &d : m_diagnostics)
        {
            os << formatOneLineWithColor(d) << '\n';
        }
        if (!m_diagnostics.empty())
        {
            const int e = errorCount();
            const int w = warningCount();
            const char *color = (e > 0) ? kRed : kYellow;
            os << color
               << "--- " << totalCount() << " diagnostic"
               << (totalCount() == 1 ? "" : "s")
               << " (" << e << " error" << (e == 1 ? "" : "s")
               << ", " << w << " warning" << (w == 1 ? "" : "s")
               << ")"
               << kReset << '\n';
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // toString
    // ─────────────────────────────────────────────────────────────────────────────

    std::string DiagnosticEngine::toString(InternedString s) const
    {
        if (!m_pool)
        {
            return "<interned:" + std::to_string(s.id) + ">";
        }
        return std::string(m_pool->lookupView(s));
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Location helper
    // ─────────────────────────────────────────────────────────────────────────────
    //
    // Phase 1 stub. BaseAST does not exist yet. In Phase 2 this file will
    // include "core/ast/BaseAST.hpp" and the body becomes:
    //
    //   return node ? node->loc : SourceLocation{};
    //
    // Nothing in Phase 1 calls the AST-aware overloads; every diagnostic test
    // uses the errorAt/warningAt/noteAt/hintAt variants, which pass a location
    // explicitly.

    SourceLocation locationOf(const BaseAST * /*node*/) noexcept
    {
        return SourceLocation{};
    }

} // namespace lucid::diag