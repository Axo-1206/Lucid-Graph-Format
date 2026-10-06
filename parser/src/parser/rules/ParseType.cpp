/// @file parser/src/parser/rules/ParseValue.cpp
///
/// @brief Implementation of parseValue and parseLiteral. STUB.
///
/// The real implementations parse:
///
///     value ::= literal
///             | IDENTIFIER
///             | IDENTIFIER '.' IDENTIFIER
///             | node_expr
///
///     literal ::= INT_LIT | FLOAT_LIT | STRING_LIT
///               | CHAR_LIT | BOOL_LIT | NIL_LIT
///
/// The value parser dispatches on the current token: a literal token
/// produces a LiteralValueAST; an IDENTIFIER produces an
/// IdentifierValueAST, a FieldAccessValueAST, or an InlineNodeValueAST
/// depending on what follows.
///
/// The stubs report NotImplemented and return marked nodes. They do not
/// consume tokens. Neither function returns nullptr; both are documented
/// in Parser.hpp as always returning a marked node on failure.

#include "parser/Parser.hpp"

#include "core/diagnostics/DiagCode.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    BaseAST *parseValue(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseValue: not yet implemented");

        UnknownAST *node = ctx.arena.make<UnknownAST>();
        // UnknownAST's constructor already sets hasSyntaxError = true.
        return node;
    }

    LiteralValueAST *parseLiteral(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseLiteral: not yet implemented");

        LiteralValueAST *node = ctx.arena.make<LiteralValueAST>();
        node->hasSyntaxError = true;
        return node;
    }

} // namespace lucid::parser