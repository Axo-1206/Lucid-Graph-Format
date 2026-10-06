/// @file parser/src/parser/rules/ParseComposite.cpp
///
/// @brief The composite parser and its field parsers.
///
/// ─── The productions ──────────────────────────────────────────────────────
/// The grammar writes:
///
///     composite_decl ::= attribute_list 'composite' IDENTIFIER '{'
///                          [ input_block ]
///                          [ output_block ]
///                          { composite_body_decl }
///                        '}'
///
///     input_block      ::= 'input' '{' { composite_field } '}'
///     output_block     ::= 'output' '{' { composite_output } '}'
///     composite_field  ::= IDENTIFIER ':' type_id
///     composite_output ::= IDENTIFIER ':' type_id '=' value
///
///     composite_body_decl ::= import_decl
///                           | enum_decl
///                           | resource_decl
///                           | node_decl
///
/// ─── The body dispatch ────────────────────────────────────────────────────
/// The composite body dispatches to the same five declaration parsers as
/// the top level, minus `composite_decl`. The shared dispatch lives in
/// `parseDeclByKeyword` (ParseDeclInternal.hpp), which rejects
/// `composite` because the caller never passes it.
///
/// ─── The composite-body recovery context ──────────────────────────────────
/// The composite body is the second (and last) recovery context in the
/// grammar. When a body declaration fails, the body loop skips to the
/// next plausible body-declaration start, or to the composite's closing
/// `}`. Unlike the top-level context, the composite body is *already*
/// inside braces, so a stray `{` inside the body is a real `{` that
/// needs its own recovery, and the `}` is a boundary the scan stops on,
/// not a foreign closer to recover upward from.

#include "parser/Parser.hpp"
#include "parser/rules/ParseDeclInternal.hpp"

#include "core/diagnostics/DiagCode.hpp"
#include "parser/support/ErrorRecovery.hpp"
#include "parser/support/GrammarPositions.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    // =============================================================================
    // parseCompositeDecl
    // =============================================================================

    CompositeDeclAST *parseCompositeDecl(TokenStream &stream,
                                         ParserContext &ctx)
    {
        const SourceLocation startLoc = stream.currentLoc();
        stream.consume(); // the `composite`

        // ─── The name ──────────────────────────────────────────────────────
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedCompositeName,
                             stream.currentLoc(),
                             "expected an identifier after 'composite'");
            return nullptr;
        }

        const InternedString name = stream.peekValue();
        stream.consume();

        // ─── The opening brace ─────────────────────────────────────────────
        if (!stream.match(TokenType::LBRACE))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedCompositeBody,
                             stream.currentLoc(),
                             "expected '{' after the composite name");
            return nullptr;
        }

        // ─── The optional input block ──────────────────────────────────────
        ArenaSpan<CompositeInputAST *> inputs{};
        if (stream.check(TokenType::KW_INPUT))
        {
            stream.consume();
            if (!stream.match(TokenType::LBRACE))
            {
                ctx.diag.errorAt(DiagCode::Syntax_ExpectedInputBlock,
                                 stream.currentLoc(),
                                 "expected '{' after 'input'");
            }
            else
            {
                auto builder = ctx.arena.makeBuilder<CompositeInputAST *>();
                while (!stream.check(TokenType::RBRACE) &&
                       !stream.isAtEnd() && ctx.canContinue())
                {
                    if (!stream.check(TokenType::IDENTIFIER))
                    {
                        ctx.diag.errorAt(DiagCode::Syntax_ExpectedInputField,
                                         stream.currentLoc(),
                                         "expected an input field name");
                        synchronizeTo(stream,
                                      TokenType::IDENTIFIER,
                                      TokenType::RBRACE);
                        if (!stream.check(TokenType::IDENTIFIER))
                        {
                            break;
                        }
                    }
                    CompositeInputAST *field =
                        parseCompositeInput(stream, ctx);
                    if (field)
                    {
                        builder.push_back(field);
                    }
                    else
                    {
                        synchronizeTo(stream,
                                      TokenType::IDENTIFIER,
                                      TokenType::RBRACE);
                        if (!stream.check(TokenType::IDENTIFIER))
                        {
                            break;
                        }
                    }
                }
                if (!stream.match(TokenType::RBRACE))
                {
                    ctx.diag.errorAt(DiagCode::Syntax_ExpectedClosing,
                                     stream.currentLoc(),
                                     "expected '}' to close the input block");
                }
                inputs = builder.build();
            }
        }

        // ─── The optional output block ─────────────────────────────────────
        ArenaSpan<CompositeOutputAST *> outputs{};
        if (stream.check(TokenType::KW_OUTPUT))
        {
            stream.consume();
            if (!stream.match(TokenType::LBRACE))
            {
                ctx.diag.errorAt(DiagCode::Syntax_ExpectedOutputBlock,
                                 stream.currentLoc(),
                                 "expected '{' after 'output'");
            }
            else
            {
                auto builder = ctx.arena.makeBuilder<CompositeOutputAST *>();
                while (!stream.check(TokenType::RBRACE) &&
                       !stream.isAtEnd() && ctx.canContinue())
                {
                    if (!stream.check(TokenType::IDENTIFIER))
                    {
                        ctx.diag.errorAt(DiagCode::Syntax_ExpectedOutputField,
                                         stream.currentLoc(),
                                         "expected an output field name");
                        synchronizeTo(stream,
                                      TokenType::IDENTIFIER,
                                      TokenType::RBRACE);
                        if (!stream.check(TokenType::IDENTIFIER))
                        {
                            break;
                        }
                    }
                    CompositeOutputAST *field =
                        parseCompositeOutput(stream, ctx);
                    if (field)
                    {
                        builder.push_back(field);
                    }
                    else
                    {
                        synchronizeTo(stream,
                                      TokenType::IDENTIFIER,
                                      TokenType::RBRACE);
                        if (!stream.check(TokenType::IDENTIFIER))
                        {
                            break;
                        }
                    }
                }
                if (!stream.match(TokenType::RBRACE))
                {
                    ctx.diag.errorAt(DiagCode::Syntax_ExpectedClosing,
                                     stream.currentLoc(),
                                     "expected '}' to close the output block");
                }
                outputs = builder.build();
            }
        }

        // ─── The body: composite_body_decl sequence ────────────────────────
        auto body = ctx.arena.makeBuilder<DeclAST *>();

        // The composite-body stop set. Stops on any token that can begin
        // a body declaration, at any depth, and on the composite's own
        // closing `}` — which ends the body loop.
        const auto bodyStop = [](TokenStream &s, int)
        {
            if (s.peekType() == TokenType::RBRACE)
            {
                return true; // the composite's closing brace
            }
            return canStartCompositeBodyDecl(s.peekType());
        };

        while (!stream.check(TokenType::RBRACE) && !stream.isAtEnd() &&
               ctx.canContinue())
        {
            if (!canStartCompositeBodyDecl(stream.peekType()))
            {
                ctx.diag.errorAt(DiagCode::Syntax_ExpectedDeclaration,
                                 stream.currentLoc(),
                                 "expected a declaration in the composite body");
                const SyncResult result =
                    synchronizeUntilDepth(stream, bodyStop);
                if (result != SyncResult::Matched)
                {
                    break; // reached end, or a foreign closer we cannot recover from
                }
                // If the scan stopped on `}`, exit the loop; the closing
                // brace handling below will consume it.
                if (stream.check(TokenType::RBRACE))
                {
                    break;
                }
            }

            // ─── The attribute list ────────────────────────────────────────
            // The body accepts attributes on declarations (see the
            // GrammarPositions.hpp doc comment for composite_body_decl for
            // the rationale). Consume them; the specific declaration
            // parsers do not see `@`.
            ArenaSpan<AttributeAST *> attrs = parseAttributeList(stream, ctx);

            // ─── The declaration ───────────────────────────────────────────
            DeclAST *decl = parseDeclByKeyword(stream, ctx);
            if (decl)
            {
                // Attach the attributes to the declaration. The
                // DeclAST::attributes field was set to empty by the
                // specific parser; replace it if we read any.
                if (!attrs.empty())
                {
                    decl->attributes = attrs;
                }
                body.push_back(decl);
            }
            else
            {
                // parseDeclByKeyword returned nullptr; skip to the next
                // plausible body declaration or the closing brace.
                const SyncResult result =
                    synchronizeUntilDepth(stream, bodyStop);
                if (result != SyncResult::Matched)
                {
                    break;
                }
                if (stream.check(TokenType::RBRACE))
                {
                    break;
                }
            }
        }

        // ─── The composite's closing `}` ───────────────────────────────────
        if (!stream.match(TokenType::RBRACE))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedClosing,
                             stream.currentLoc(),
                             "expected '}' to close the composite body");
        }

        CompositeDeclAST *node = ctx.arena.make<CompositeDeclAST>(
            name, inputs, outputs, body.build());
        node->loc = startLoc;
        return node;
    }

    // =============================================================================
    // parseCompositeInput
    // =============================================================================

    CompositeInputAST *parseCompositeInput(TokenStream &stream,
                                           ParserContext &ctx)
    {
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedInputField,
                             stream.currentLoc(),
                             "expected an input field name");
            return nullptr;
        }

        const SourceLocation startLoc = stream.currentLoc();
        const InternedString name = stream.peekValue();
        stream.consume();

        if (!stream.match(TokenType::COLON))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedFieldType,
                             stream.currentLoc(),
                             "expected ':' after the input name");
            CompositeInputAST *node =
                ctx.arena.make<CompositeInputAST>(name, nullptr);
            node->loc = startLoc;
            node->hasSyntaxError = true;
            return node;
        }

        TypeIdAST *type = parseTypeId(stream, ctx);

        CompositeInputAST *node =
            ctx.arena.make<CompositeInputAST>(name, type);
        node->loc = startLoc;
        if (type && type->hasSyntaxError)
        {
            node->hasSyntaxError = true;
        }
        return node;
    }

    // =============================================================================
    // parseCompositeOutput
    // =============================================================================

    CompositeOutputAST *parseCompositeOutput(TokenStream &stream,
                                             ParserContext &ctx)
    {
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedOutputField,
                             stream.currentLoc(),
                             "expected an output field name");
            return nullptr;
        }

        const SourceLocation startLoc = stream.currentLoc();
        const InternedString name = stream.peekValue();
        stream.consume();

        if (!stream.match(TokenType::COLON))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedFieldType,
                             stream.currentLoc(),
                             "expected ':' after the output name");
            CompositeOutputAST *node =
                ctx.arena.make<CompositeOutputAST>(name, nullptr, nullptr);
            node->loc = startLoc;
            node->hasSyntaxError = true;
            return node;
        }

        TypeIdAST *type = parseTypeId(stream, ctx);

        if (!stream.match(TokenType::EQUALS))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedOutputBinding,
                             stream.currentLoc(),
                             "expected '=' after the output type");
            CompositeOutputAST *node =
                ctx.arena.make<CompositeOutputAST>(name, type, nullptr);
            node->loc = startLoc;
            node->hasSyntaxError = true;
            return node;
        }

        // ─── The value ─────────────────────────────────────────────────────
        // The grammar's `composite_output` uses `value`, not `literal`.
        // A composite output can bind to a resource field, a node output,
        // or a composite input — all value forms.
        BaseAST *value = parseValue(stream, ctx);

        CompositeOutputAST *node =
            ctx.arena.make<CompositeOutputAST>(name, type, value);
        node->loc = startLoc;
        if ((type && type->hasSyntaxError) ||
            (value && value->hasSyntaxError))
        {
            node->hasSyntaxError = true;
        }
        return node;
    }

} // namespace lucid::parser