/// @file parser/src/parser/rules/ParseDeclInternal.hpp
///
/// @brief Internal declarations shared between the parser's rules/ files.
///
/// ─── What this file is for ────────────────────────────────────────────────
/// `Parser.hpp` is the parser's public API. A function declared there is
/// callable by anything linking `lucid_parser`. That is correct for
/// `parseFile`, `parseTypeId`, `parseValue`, and the other entry points
/// the tests use.
///
/// Some functions are shared between two `rules/` files but are not part
/// of the public API. `parseDeclByKeyword` is the first: the top-level
/// dispatch in `Parser.cpp` uses it.
///
/// Internal headers are not listed in any CMake target (they are
/// headers), and they are included only by the `.cpp` files that need
/// them.

#pragma once

#include "core/ast/DeclAST.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"

namespace lucid::parser
{

    /// @brief Dispatch to a declaration parser based on the current token.
    ///
    /// Preconditions:
    ///   - Any attribute list has already been consumed by the caller.
    ///   - The current token is a declaration keyword (`import`, `enum`,
    ///     `resource`, `node`).
    ///
    /// Postconditions:
    ///   - On success, the current token is the first token after the
    ///     declaration, and the returned node is a concrete declaration
    ///     of the matching kind.
    ///   - On failure, a diagnostic has been reported and `nullptr` is
    ///     returned.
    DeclAST *parseDeclByKeyword(TokenStream &stream, ParserContext &ctx);

} // namespace lucid::parser