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
/// dispatch in `Parser.cpp` and the composite-body dispatch in
/// `ParseComposite.cpp` both need "which declaration parser for this
/// keyword", and duplicating the switch in two files would mean updating
/// two places when the grammar grows a sixth declaration kind.
///
/// Internal headers are not listed in any CMake target (they are
/// headers), and they are included only by the `.cpp` files that need
/// them. A function declared here is not part of the parser's public
/// surface.
///
/// ─── Why a header and not a comment in Parser.hpp ─────────────────────────
/// A reader of `Parser.hpp` should see the parser's API. A reader of a
/// `.cpp` should see its file's internals. A shared internal function
/// belongs to neither of those files individually; it belongs to the
/// header that groups the internal interfaces of its production family.

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
    ///     `resource`, `node`) or `composite` (`composite` is rejected by
    ///     the composite-body caller, but this function accepts it because
    ///     the top-level caller needs it).
    ///
    /// Postconditions:
    ///   - On success, the current token is the first token after the
    ///     declaration, and the returned node is a concrete declaration
    ///     of the matching kind.
    ///   - On failure, a diagnostic has been reported and `nullptr` is
    ///     returned. The caller runs a synchronizer.
    ///
    /// The caller attaches the attribute list to the returned node. This
    /// function does not see attributes; it is called after they are
    /// consumed.
    DeclAST *parseDeclByKeyword(TokenStream &stream, ParserContext &ctx);

} // namespace lucid::parser