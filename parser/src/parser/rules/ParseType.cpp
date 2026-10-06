/// @file parser/src/parser/rules/ParseType.cpp
///
/// @brief Implementation of parseTypeId.
///
/// ─── The production ───────────────────────────────────────────────────────
/// The grammar's §2.5 writes:
///
///     type_id ::= IDENTIFIER [ '.' IDENTIFIER ]
///
/// A type reference is a name, optionally with one level of module
/// qualification. `Key` refers to a type in the local scope; `core.Key`
/// refers to a type from the module imported as `core`.
///
/// ─── What the parser does not do ──────────────────────────────────────────
/// It does not resolve the name. Whether `Key` is a primitive, an enum, a
/// handle, or nothing at all is a registry fact and a Sema concern. The
/// parser produces a TypeIdAST and moves on.
///
/// It does not accept more than one level of qualification. `a.b.c` is a
/// syntax error under the current grammar (see §8.2 of the grammar for the
/// open question). The parser returns `a.b` as a valid node and reports
/// the second `.` as unexpected; the caller's recovery handles the
/// remainder.
///
/// ─── Error behavior ───────────────────────────────────────────────────────
/// Three failure modes, all partial-parse (the function never returns
/// nullptr):
///
///   1. No identifier at all → Syntax_ExpectedIdentifier, marked node with
///      an invalid name. Nothing consumed.
///
///   2. A `.` not followed by an identifier → Syntax_ExpectedFieldAccess,
///      marked node with a valid qualifier and an invalid name. The `.` is
///      consumed; nothing else is.
///
///   3. A second `.` after a valid qualified name → Syntax_UnexpectedToken,
///      the valid node is returned, the second `.` is not consumed.

#include "parser/Parser.hpp"

#include "core/diagnostics/DiagCode.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    TypeIdAST *parseTypeId(TokenStream &stream, ParserContext &ctx)
    {
        // ─── The mandatory identifier ──────────────────────────────────────
        //
        // If the current token is not an IDENTIFIER, there is no type name
        // to record. Report and return a marked, empty node. Consume
        // nothing: the caller's recovery decides what to do with the
        // unexpected token.

        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedIdentifier,
                             stream.currentLoc(),
                             "expected a type name");

            TypeIdAST *node = ctx.arena.make<TypeIdAST>();
            node->hasSyntaxError = true;
            return node;
        }

        const SourceLocation startLoc = stream.currentLoc();
        const InternedString first = stream.peekValue();
        stream.consume();

        // ─── The optional qualifier ────────────────────────────────────────
        //
        // A `.` immediately after the first identifier means a qualified
        // name. It must be followed by a second identifier. If it is not,
        // the `.` is consumed (to avoid an infinite loop in the caller's
        // recovery) and the node is marked.

        if (!stream.check(TokenType::DOT))
        {
            // Unqualified: `Key`.
            TypeIdAST *node = ctx.arena.make<TypeIdAST>(first);
            node->loc = startLoc;
            return node;
        }

        // Consume the `.`.
        stream.consume();

        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedFieldAccess,
                             stream.currentLoc(),
                             "expected a type name after '.'");

            TypeIdAST *node = ctx.arena.make<TypeIdAST>(first, InternedString{});
            node->loc = startLoc;
            node->hasSyntaxError = true;
            return node;
        }

        const InternedString second = stream.peekValue();
        stream.consume();

        // ─── The qualified node ────────────────────────────────────────────
        //
        // `first` is the qualifier, `second` is the name. Note the argument
        // order: the two-argument TypeIdAST constructor is
        // (qualifier, name).

        TypeIdAST *node = ctx.arena.make<TypeIdAST>(first, second);
        node->loc = startLoc;

        // ─── The forbidden third segment ───────────────────────────────────
        //
        // The grammar allows at most one `.`. A second `.` is a syntax
        // error under the current grammar. Report it but do not consume
        // it: the node `core.Key` is valid, and the caller's recovery will
        // handle whatever follows the extra dot. See §8.2 of the grammar.

        if (stream.check(TokenType::DOT))
        {
            ctx.diag.errorAt(DiagCode::Syntax_UnexpectedToken,
                             stream.currentLoc(),
                             "a type name may have at most one '.' qualifier");
            node->hasSyntaxError = true;
        }

        return node;
    }

} // namespace lucid::parser