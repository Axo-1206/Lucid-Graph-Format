/// @file parser/dump/JSONDumper.hpp
///
/// @brief AST → JSON for the Lucid Graph Format.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A serializer that walks a parsed ModuleAST and produces a JSON text
/// representation. The JSON is the format the parser's fixture tests
/// compare against, and the format `lucid dump` prints for inspection.
///
/// ─── What this is not ─────────────────────────────────────────────────────
/// It is not a general-purpose AST serializer. It has no reader, no
/// schema, no versioning, and no round-trip guarantee. If a consumer
/// wants a ModuleAST, they call parseFile. If they want to inspect the
/// AST, they read this JSON, as JSON, following the field names the AST
/// headers declare.
///
/// ─── The shape ────────────────────────────────────────────────────────────
/// The output is a single JSON object:
///
///   {
///     "filePath": "test.lucid",
///     "declarations": [ ... ]
///   }
///
/// Every node is a flat object with a "kind" string and its own fields.
/// Field names match the AST node's C++ field names. Interned strings
/// are written as their text. Optional fields (null pointers, absent
/// qualifiers) are written as JSON null. `hasSyntaxError` is present
/// only when true.
///
/// ─── Determinism ──────────────────────────────────────────────────────────
/// The output is deterministic: the same AST produces the same bytes,
/// every time. This is what makes it suitable for fixture comparison.
/// The dumper does not sort anything; it emits fields in a fixed order
/// and arrays in source order.
///
/// ─── No pretty printing ───────────────────────────────────────────────────
/// The output is compact. Human readers open it in an editor, which
/// pretty-prints it. The fixture tests compare the compact text directly.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/memory/StringPool.hpp"

#include <string>

namespace lucid::parser::dump
{

    /// @brief Serialize a ModuleAST to JSON.
    ///
    /// The module pointer may be null; a null module is written as
    /// JSON null.
    ///
    /// @param module The module to serialize.
    /// @param pool   The StringPool that interned the module's strings.
    /// @return The JSON text.
    std::string dumpModule(const ModuleAST *module, const StringPool &pool);

} // namespace lucid::parser::dump