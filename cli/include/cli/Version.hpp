/// @file cli/Version.hpp
///
/// @brief The CLI tools' version string.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// The version string printed by `lucid-fmt --version` and
/// `lucid-check --version`. It is a single constant so the two tools
/// cannot drift.
///
/// ─── When to change it ────────────────────────────────────────────────────
/// Change it when the tools' behavior changes in a user-visible way.
/// The version is not tied to a git tag or a build number; it is a
/// hand-maintained string.

#pragma once

namespace lucid::cli
{

    /// The version string for all CLI tools.
    inline constexpr const char *kVersion = "0.1.0";

} // namespace lucid::cli