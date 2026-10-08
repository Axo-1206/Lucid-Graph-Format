/// @file parser/src/parser/rules/ParseNode.cpp
///
/// @brief Implementation of parseNodeExpr, parseArgList, parseTriggerList.
///
/// ─── The productions ──────────────────────────────────────────────────────
/// The grammar's §2.6 writes:
///
///     node_expr    ::= NodeType '(' [ arg_list ] ')'
///     NodeType     ::= [ IDENTIFIER '::' ] IDENTIFIER
///     arg_list     ::= arg { ',' arg } [ ',' ]
///     trigger_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]
///
/// ─── node_expr ────────────────────────────────────────────────────────────
/// parseNodeExpr parses the NodeType with parseTypeId, then expects `(`,
/// then calls parseArgList (which consumes the closing `)`), then builds
/// a NodeExprAST.
///
/// The NodeType is parsed by parseTypeId, so a qualified node type
/// (`physics::Body`) and an unqualified one (`Float32Node`) go through
/// the same path. parseTypeId handles the `::` qualifier; this function
/// does not see the separator.
///
/// Error behavior:
///   - Missing node type → parseTypeId returns a marked TypeIdAST; the
///     enclosing NodeExprAST is marked and returned. Nothing else is
///     consumed.
///   - Missing `(` → report Syntax_ExpectedNodeArgList and return the
///     marked NodeExprAST with the parsed type. Nothing after the type
///     is consumed.
///   - Missing `)` → parseArgList reports and returns the arguments read
///     so far. The enclosing NodeExprAST is marked.
///
/// ─── arg_list ─────────────────────────────────────────────────────────────
/// parseArgList is called with the `(` already consumed. It parses a
/// comma-separated list of values (allowing a trailing comma) and
/// consumes the closing `)`.
///
/// Error behavior:
///   - Missing `)` → report Syntax_ExpectedClosing and return the
///     arguments read so far.
///   - A value fails to parse → parseValue returns a marked UnknownAST;
///     the argument is included in the list and the enclosing node is
///     marked.
///
/// ─── trigger_list ─────────────────────────────────────────────────────────
/// parseTriggerList is called with the `on` already consumed. It parses
/// a comma-separated list of identifiers (allowing a trailing comma).
/// There is no closing token; the list ends when the next token is not
/// an identifier or a comma.
///
/// Error behavior:
///   - Missing first trigger → report Syntax_ExpectedTriggerList and
///     return an empty span.

#include "parser/Parser.hpp"

#include "core/diagnostics/DiagCode.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    // =============================================================================
    // parseNodeExpr
    // =============================================================================

    NodeExprAST *parseNodeExpr(TokenStream &stream, ParserContext &ctx)
    {
        const SourceLocation startLoc = stream.currentLoc();

        // ─── The node type ─────────────────────────────────────────────────
        // parseTypeId handles the optional `::` qualifier. An unqualified
        // type (`Float32Node`) and a qualified type (`physics::Body`) both
        // arrive here as a single TypeIdAST.
        TypeIdAST *type = parseTypeId(stream, ctx);

        // ─── The argument list ─────────────────────────────────────────────
        if (!stream.check(TokenType::LPAREN))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedNodeArgList,
                             stream.currentLoc(),
                             "expected '(' after the node type");

            NodeExprAST *node =
                ctx.arena.make<NodeExprAST>(type, ArenaSpan<BaseAST *>{});
            node->loc = startLoc;
            node->hasSyntaxError = true;
            return node;
        }

        stream.consume(); // the `(`
        ArenaSpan<BaseAST *> args = parseArgList(stream, ctx);

        NodeExprAST *node = ctx.arena.make<NodeExprAST>(type, args);
        node->loc = startLoc;

        // Propagate markedness from the type and from any argument. A marked
        // argument (parseValue returns a marked UnknownAST when the current
        // token cannot begin a value) makes the whole node expression marked,
        // per the "flag propagates upward" rule in BaseAST.hpp.
        if (type && type->hasSyntaxError)
        {
            node->hasSyntaxError = true;
        }
        for (const BaseAST *arg : args)
        {
            if (arg && arg->hasSyntaxError)
            {
                node->hasSyntaxError = true;
                break;
            }
        }
        return node;
    }

    // =============================================================================
    // parseArgList
    // =============================================================================

    ArenaSpan<BaseAST *> parseArgList(TokenStream &stream, ParserContext &ctx)
    {
        // The caller has consumed the `(`. If the next token is `)`, the
        // list is empty.
        if (stream.check(TokenType::RPAREN))
        {
            stream.consume();
            return {};
        }

        auto builder = ctx.arena.makeBuilder<BaseAST *>();

        // ─── First argument ────────────────────────────────────────────────
        builder.push_back(parseValue(stream, ctx));

        // ─── Subsequent arguments ──────────────────────────────────────────
        while (stream.match(TokenType::COMMA))
        {
            // A trailing comma before `)`.
            if (stream.check(TokenType::RPAREN))
            {
                break;
            }

            builder.push_back(parseValue(stream, ctx));
        }

        // ─── The closing `)` ───────────────────────────────────────────────
        if (!stream.match(TokenType::RPAREN))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedClosing,
                             stream.currentLoc(),
                             "expected ')' to close the argument list");
        }

        return builder.build();
    }

    // =============================================================================
    // parseTriggerList
    // =============================================================================

    ArenaSpan<InternedString> parseTriggerList(TokenStream &stream,
                                               ParserContext &ctx)
    {
        // The caller has consumed `on`. The list begins with an
        // identifier; if it does not, the trigger list is empty and the
        // `on` was superfluous.
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedTriggerList,
                             stream.currentLoc(),
                             "expected a trigger name after 'on'");
            return {};
        }

        auto builder = ctx.arena.makeBuilder<InternedString>();

        // ─── First trigger ─────────────────────────────────────────────────
        builder.push_back(stream.peekValue());
        stream.consume();

        // ─── Subsequent triggers ───────────────────────────────────────────
        while (stream.match(TokenType::COMMA))
        {
            // A trailing comma: the next token is not an identifier, so
            // the list is done. The comma is consumed.
            if (!stream.check(TokenType::IDENTIFIER))
            {
                break;
            }

            builder.push_back(stream.peekValue());
            stream.consume();
        }

        return builder.build();
    }

} // namespace lucid::parser