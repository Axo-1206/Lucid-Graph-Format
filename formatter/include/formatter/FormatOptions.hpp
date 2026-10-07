/// @file formatter/FormatOptions.hpp
///
/// @brief Tunables for the formatter.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A plain value type. The formatter reads its fields; no method is
/// called on it. The defaults are the ones every fixture uses.
///
/// ─── What this is not ─────────────────────────────────────────────────────
/// It is not a style-sheet. The formatter has one canonical layout per
/// construct, and it does not take an opinion on bracket placement, line
/// breaking, or quote style. The two fields here are the only choices a
/// caller gets: how wide is an indent, and are indents tabs or spaces.
///
/// Line length is not here because Phase 5 does not do line breaking.
/// If it is added later, it becomes a third field, with a default of 100.

#pragma once

#include <cstdint>

namespace lucid::formatter
{

    /// @brief The formatter's configuration.
    struct FormatOptions
    {
        /// The number of columns per indent level. Ignored if
        /// `use_tabs` is true; a tab counts as one indent level.
        ///
        /// The default is four spaces, matching the fixtures.
        int indent_width = 4;

        /// If true, indentation uses a single tab per level instead of
        /// `indent_width` spaces. The default is spaces.
        bool use_tabs = false;
    };

} // namespace lucid::formatter