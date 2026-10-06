/// @file parser/src/parser/rules/ParseNode.cpp
///
/// @brief Implementation of parseNodeExpr, parseArgList, parseTriggerList.
///        STUB.
///
/// The real implementations parse:
///
///     node_expr    ::= NodeType '(' [ arg_list ] ')'
///     NodeType     ::= IDENTIFIER [ '.' IDENTIFIER ]
///     arg_list     ::= arg { ',' arg } [ ',' ]
///     trigger_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]
///
/// The arg list and trigger list parsers are the "sub-parse" helpers the
/// legacy design put in Helpers.cpp. Under the new grammar they live
/// here, in the file for the production they belong to: they are
/// node-expression concerns, not general parser helpers.
///
/// The stubs report NotImplemented and return marked / empty results.
/// They do not consume tokens. `parseNodeExpr` never returns nullptr;
/// the two helpers return empty spans on failure.

#include "parser/Parser.hpp"

#include "core/diagnostics/DiagCode.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    NodeExprAST *parseNodeExpr(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseNodeExpr: not yet implemented");

        NodeExprAST *node = ctx.arena.make<NodeExprAST>();
        node->hasSyntaxError = true;
        return node;
    }

    ArenaSpan<BaseAST *> parseArgList(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseArgList: not yet implemented");

        return {};
    }

    ArenaSpan<InternedString> parseTriggerList(TokenStream &stream,
                                               ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseTriggerList: not yet implemented");

        return {};
    }

} // namespace lucid::parser