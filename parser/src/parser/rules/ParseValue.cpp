/// @file parser/src/parser/rules/ParseValue.cpp
///
/// @brief Implementation of parseValue and parseLiteral.
///
/// ─── The production ───────────────────────────────────────────────────────
/// The grammar's §2.7 writes:
///
///     value ::= literal
///             | IDENTIFIER
///             | IDENTIFIER '.' IDENTIFIER
///             | node_expr
///
///     literal ::= INT_LIT | FLOAT_LIT | STRING_LIT
///               | CHAR_LIT | BOOL_LIT | NIL_LIT
///
/// ─── The dispatch ─────────────────────────────────────────────────────────
/// The current token determines the form:
///
///   - A literal token → parseLiteral → LiteralValueAST.
///
///   - An IDENTIFIER whose next token is `(` → parseNodeExpr →
///     InlineNodeValueAST wrapping a NodeExprAST. The check happens
///     before consuming the identifier, so parseNodeExpr sees it.
///
///   - An IDENTIFIER whose next token is `::` and whose token after that
///     is an IDENTIFIER followed by `(` → a qualified inline node:
///     `Module::Type(...)` → InlineNodeValueAST wrapping a NodeExprAST.
///
///   - An IDENTIFIER whose next token is `.` → FieldAccessValueAST.
///
///   - An IDENTIFIER otherwise → IdentifierValueAST.
///
///   - Anything else → a marked UnknownAST, nothing consumed.
///
/// ─── Dot vs. double-colon ─────────────────────────────────────────────────
/// A dot (`.`) is field access: `Object.Field`. A double-colon (`::`) is
/// module qualification: `Module::Type`. They never mix in one name. A
/// qualified inline node is `Module::Type(...)`, not `Module.Type(...)`.
/// The latter parses as a field access `Module.Type` followed by a `(`,
/// which is a syntax error at the call site.
///
/// ─── The inline-node lookahead ────────────────────────────────────────────
/// The "identifier followed by `(`" case needs one token of lookahead.
/// The qualified case (`Module::Type(...)`) needs four: the current
/// identifier, the `::`, the second identifier, and the `(`. The parser
/// does this with peekAt rather than consuming and rewinding, because the
/// rewind is uglier for the same result. This is bounded lookahead, not
/// backtracking; the grammar remains LL(1) in the sense that no parse
/// decision depends on more than one token of *dispatch* input at each
/// decision point.

#include "parser/Parser.hpp"

#include "core/diagnostics/DiagCode.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    // =============================================================================
    // parseValue
    // =============================================================================

    BaseAST *parseValue(TokenStream &stream, ParserContext &ctx)
    {
        const TokenType current = stream.peekType();

        // ─── Literal ───────────────────────────────────────────────────────
        if (isLiteral(current))
        {
            return parseLiteral(stream, ctx);
        }

        // ─── Identifier-based forms ────────────────────────────────────────
        if (current == TokenType::IDENTIFIER)
        {
            const TokenType follower = stream.peekNextType();

            // Unqualified inline node: `TypeName(...)`.
            if (follower == TokenType::LPAREN)
            {
                NodeExprAST *expr = parseNodeExpr(stream, ctx);
                InlineNodeValueAST *node =
                    ctx.arena.make<InlineNodeValueAST>(expr);
                node->loc = expr ? expr->loc : stream.currentLoc();
                if (expr && expr->hasSyntaxError)
                {
                    node->hasSyntaxError = true;
                }
                return node;
            }

            // Qualified inline node: `Module::Type(...)`.
            if (follower == TokenType::COLON_COLON &&
                stream.peekAt(2).type == TokenType::IDENTIFIER &&
                stream.peekAt(3).type == TokenType::LPAREN)
            {
                NodeExprAST *expr = parseNodeExpr(stream, ctx);
                InlineNodeValueAST *node =
                    ctx.arena.make<InlineNodeValueAST>(expr);
                node->loc = expr ? expr->loc : stream.currentLoc();
                if (expr && expr->hasSyntaxError)
                {
                    node->hasSyntaxError = true;
                }
                return node;
            }

            // A `::` that does not complete a qualified node expression.
            if (follower == TokenType::COLON_COLON)
            {
                const SourceLocation startLoc = stream.currentLoc();
                const InternedString name = stream.peekValue();
                stream.consume(); // the identifier

                ctx.diag.errorAt(DiagCode::Syntax_UnexpectedToken,
                                 stream.currentLoc(),
                                 "a '::' qualifier is only valid in a node "
                                 "expression such as Module::Type(...)");
                stream.consume(); // the '::'

                IdentifierValueAST *node =
                    ctx.arena.make<IdentifierValueAST>(name);
                node->loc = startLoc;
                node->hasSyntaxError = true;
                return node;
            }

            // Field access: `Object.Field`.
            if (follower == TokenType::DOT)
            {
                const SourceLocation startLoc = stream.currentLoc();
                const InternedString object = stream.peekValue();
                stream.consume(); // the identifier
                stream.consume(); // the dot

                if (!stream.check(TokenType::IDENTIFIER))
                {
                    ctx.diag.errorAt(DiagCode::Syntax_ExpectedFieldAccess,
                                     stream.currentLoc(),
                                     "expected a field name after '.'");
                    FieldAccessValueAST *node =
                        ctx.arena.make<FieldAccessValueAST>(
                            object, InternedString{});
                    node->loc = startLoc;
                    node->hasSyntaxError = true;
                    return node;
                }

                const InternedString field = stream.peekValue();
                stream.consume();

                FieldAccessValueAST *node =
                    ctx.arena.make<FieldAccessValueAST>(object, field);
                node->loc = startLoc;
                return node;
            }

            // Bare identifier.
            const SourceLocation startLoc = stream.currentLoc();
            const InternedString name = stream.peekValue();
            stream.consume();

            IdentifierValueAST *node =
                ctx.arena.make<IdentifierValueAST>(name);
            node->loc = startLoc;
            return node;
        }

        // ─── No value form begins with this token ──────────────────────────
        ctx.diag.errorAt(DiagCode::Syntax_ExpectedValue,
                         stream.currentLoc(),
                         "expected a value (a literal, an identifier, or a "
                         "node expression)");

        UnknownAST *node = ctx.arena.make<UnknownAST>();
        node->loc = stream.currentLoc();
        return node;
    }

    // =============================================================================
    // parseLiteral
    // =============================================================================

    LiteralValueAST *parseLiteral(TokenStream &stream, ParserContext &ctx)
    {
        const TokenType current = stream.peekType();

        if (!isLiteral(current))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedLiteral,
                             stream.currentLoc(),
                             "expected a literal");

            LiteralValueAST *node = ctx.arena.make<LiteralValueAST>();
            node->loc = stream.currentLoc();
            node->hasSyntaxError = true;
            return node;
        }

        const SourceLocation startLoc = stream.currentLoc();
        const InternedString text = stream.peekValue();
        const LiteralKind kind = literalKindOf(current);
        stream.consume();

        LiteralValueAST *node =
            ctx.arena.make<LiteralValueAST>(kind, text);
        node->loc = startLoc;
        return node;
    }

} // namespace lucid::parser