/// @file cli/Registry.hpp
///
/// @brief The default registry shared by the CLI commands.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// The node types, enum types, handle types, and phases that the CLI
/// tools recognize when no engine-specific registry is supplied. It is
/// a fixed, small vocabulary: enough to compile the fixtures and the
/// examples, not enough to run a real game.
///
/// ─── Why it is shared ─────────────────────────────────────────────────────
/// `lucid-check` and `lucid-compile` both need a registry. Neither has
/// an engine to ask for one. This header gives them the same one, so a
/// file that compiles under one tool compiles under the other.
///
/// ─── The engine supplies its own ──────────────────────────────────────────
/// A shipping engine builds its own `Registry` from its node
/// implementations and calls `compile` / `deserialize` directly. The
/// CLI's registry is a convenience for tooling, not a runtime input.

#pragma once

#include "core/memory/ArenaSpan.hpp"
#include "sema/NodeKind.hpp"
#include "sema/Registry.hpp"
#include "sema/TypeId.hpp"

namespace lucid::cli
{

    /// The default registry the CLI tools use.
    ///
    /// The returned reference is valid for the process's lifetime. The
    /// arrays backing it are function-local statics.
    const lucid::sema::Registry &defaultRegistry();

} // namespace lucid::cli