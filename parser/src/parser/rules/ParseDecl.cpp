/// @file parser/src/parser/rules/ParseDecl.cpp
///
/// @brief The declaration parsers, the attribute parsers, and the two
///        declaration-adjacent helpers.
///
/// ─── The productions ──────────────────────────────────────────────────────
/// The grammar writes:
///
///     import_decl ::= 'import' module_path [ 'as' IDENTIFIER ]
///     module_path ::= IDENTIFIER { '.' IDENTIFIER }
///
///     enum_decl ::= 'enum' IDENTIFIER '{' enum_member_list '}'
///     enum_member_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]
///
///     resource_decl  ::= attribute_list 'resource' IDENTIFIER
///                            '{' { resource_field } '}'
///     resource_field ::= IDENTIFIER ':' type_id [ '=' literal ]
///
///     node_decl ::= 'node' IDENTIFIER '=' node_expr
///                       [ 'on' trigger_list ]
///
///     attribute_list ::= { '@' IDENTIFIER }
///
/// ─── Attributes are read by parseDecl, not by these parsers ───────────────
/// `parseDecl` in Parser.cpp reads the attribute list before dispatching.
/// The specific declaration parsers below never see an `@`.
///
/// ─── The shared dispatcher ────────────────────────────────────────────────
/// `parseDeclByKeyword` is the "which parser for which keyword" switch.
/// It is shared between the top-level dispatch and the composite-body
/// dispatch. It is declared in ParseDeclInternal.hpp, not Parser.hpp,
/// because it is not part of the parser's public API.

#include "parser/Parser.hpp"
#include "parser/rules/ParseDeclInternal.hpp"

#include "core/diagnostics/DiagCode.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    // =============================================================================
    // parseDeclByKeyword — the shared dispatcher
    // =============================================================================

    DeclAST *parseDeclByKeyword(TokenStream &stream, ParserContext &ctx)
    {
        switch (stream.peekType())
        {
        case TokenType::KW_IMPORT:
            return parseImportDecl(stream, ctx);
        case TokenType::KW_ENUM:
            return parseEnumDecl(stream, ctx);
        case TokenType::KW_RESOURCE:
            return parseResourceDecl(stream, ctx);
        case TokenType::KW_NODE:
            return parseNodeDecl(stream, ctx);
        case TokenType::KW_COMPOSITE:
            return parseCompositeDecl(stream, ctx);
        default:
            // The caller has already checked canStartTopDecl (or the
            // composite-body equivalent). Reaching here is a caller bug,
            // not a user error. Report internally and return nullptr so
            // the caller's loop can recover.
            ctx.diag.errorAt(DiagCode::Internal_Assertion,
                             stream.currentLoc(),
                             "parseDeclByKeyword: caller passed a token "
                             "that does not begin a declaration");
            return nullptr;
        }
    }

    // =============================================================================
    // Import
    // =============================================================================

    ImportDeclAST *parseImportDecl(TokenStream &stream, ParserContext &ctx)
    {
        const SourceLocation startLoc = stream.currentLoc();
        stream.consume(); // the `import`

        // ─── The module path ───────────────────────────────────────────────
        InternedString path = parseModulePath(stream, ctx);
        if (!path.isValid())
        {
            // parseModulePath already reported; there is nothing to
            // build a usable import from.
            return nullptr;
        }

        // ─── The optional alias ────────────────────────────────────────────
        // `as` is matched by spelling, not by keyword. The lexer produces
        // IDENTIFIER for it, per Tokens.hpp's design note.
        InternedString alias{};
        if (stream.check(TokenType::IDENTIFIER) &&
            stream.peekValueView(ctx.pool) == std::string_view{"as"})
        {
            stream.consume(); // `as`

            if (!stream.check(TokenType::IDENTIFIER))
            {
                ctx.diag.errorAt(DiagCode::Syntax_ExpectedImportAlias,
                                 stream.currentLoc(),
                                 "expected an identifier after 'as'");
                // Fall through: use the last path segment as the alias.
            }
            else
            {
                alias = stream.peekValue();
                stream.consume();
            }
        }

        // ─── Default alias: the last path segment ──────────────────────────
        if (!alias.isValid())
        {
            const std::string_view fullPath = ctx.pool.lookupView(path);
            const size_t lastDot = fullPath.rfind('.');
            const std::string_view lastSegment =
                lastDot == std::string_view::npos
                    ? fullPath
                    : fullPath.substr(lastDot + 1);
            alias = ctx.pool.intern(lastSegment);
        }

        ImportDeclAST *node = ctx.arena.make<ImportDeclAST>(path, alias);
        node->loc = startLoc;
        return node;
    }

    InternedString parseModulePath(TokenStream &stream, ParserContext &ctx)
    {
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedModulePath,
                             stream.currentLoc(),
                             "expected a module path after 'import'");
            return InternedString{};
        }

        // Accumulate the path as a single string. The path segments are
        // joined with `.`.
        std::string path(stream.peekValueView(ctx.pool));
        stream.consume();

        while (stream.check(TokenType::DOT))
        {
            stream.consume(); // `.`

            if (!stream.check(TokenType::IDENTIFIER))
            {
                ctx.diag.errorAt(DiagCode::Syntax_ExpectedModulePath,
                                 stream.currentLoc(),
                                 "expected a module path segment after '.'");
                break;
            }

            path += '.';
            path += stream.peekValueView(ctx.pool);
            stream.consume();
        }

        return ctx.pool.intern(path);
    }

    // =============================================================================
    // Enum
    // =============================================================================

    EnumDeclAST *parseEnumDecl(TokenStream &stream, ParserContext &ctx)
    {
        const SourceLocation startLoc = stream.currentLoc();
        stream.consume(); // the `enum`

        // ─── The name ──────────────────────────────────────────────────────
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedEnumName,
                             stream.currentLoc(),
                             "expected an identifier after 'enum'");
            return nullptr;
        }

        const InternedString name = stream.peekValue();
        stream.consume();

        // ─── The body ──────────────────────────────────────────────────────
        if (!stream.match(TokenType::LBRACE))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedEnumBody,
                             stream.currentLoc(),
                             "expected '{' after the enum name");
            return nullptr;
        }

        ArenaSpan<InternedString> members = parseEnumMemberList(stream, ctx);

        EnumDeclAST *node = ctx.arena.make<EnumDeclAST>(name, members);
        node->loc = startLoc;
        return node;
    }

    ArenaSpan<InternedString> parseEnumMemberList(TokenStream &stream,
                                                  ParserContext &ctx)
    {
        // The caller has consumed `{`. If the next token is `}`, the
        // list is empty (which Sema may later reject as a warning).
        if (stream.check(TokenType::RBRACE))
        {
            stream.consume();
            return {};
        }

        auto builder = ctx.arena.makeBuilder<InternedString>();

        // ─── First member ──────────────────────────────────────────────────
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedEnumMember,
                             stream.currentLoc(),
                             "expected an enum member name");
            // Fall through: try to consume the closing `}`.
            stream.match(TokenType::RBRACE);
            return builder.build();
        }

        builder.push_back(stream.peekValue());
        stream.consume();

        // ─── Subsequent members ────────────────────────────────────────────
        while (stream.match(TokenType::COMMA))
        {
            // A trailing comma before `}`.
            if (stream.check(TokenType::RBRACE))
            {
                break;
            }

            if (!stream.check(TokenType::IDENTIFIER))
            {
                ctx.diag.errorAt(DiagCode::Syntax_ExpectedEnumMember,
                                 stream.currentLoc(),
                                 "expected an enum member name after ','");
                break;
            }

            builder.push_back(stream.peekValue());
            stream.consume();
        }

        // ─── The closing `}` ───────────────────────────────────────────────
        if (!stream.match(TokenType::RBRACE))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedClosing,
                             stream.currentLoc(),
                             "expected '}' to close the enum body");
        }

        return builder.build();
    }

    // =============================================================================
    // Resource
    // =============================================================================

    ResourceDeclAST *parseResourceDecl(TokenStream &stream, ParserContext &ctx)
    {
        const SourceLocation startLoc = stream.currentLoc();
        stream.consume(); // the `resource`

        // ─── The name ──────────────────────────────────────────────────────
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedResourceName,
                             stream.currentLoc(),
                             "expected an identifier after 'resource'");
            return nullptr;
        }

        const InternedString name = stream.peekValue();
        stream.consume();

        // ─── The body ──────────────────────────────────────────────────────
        if (!stream.match(TokenType::LBRACE))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedResourceBody,
                             stream.currentLoc(),
                             "expected '{' after the resource name");
            return nullptr;
        }

        auto fields = ctx.arena.makeBuilder<ResourceFieldAST *>();

        while (!stream.check(TokenType::RBRACE) && !stream.isAtEnd() &&
               ctx.canContinue())
        {
            if (!stream.check(TokenType::IDENTIFIER))
            {
                ctx.diag.errorAt(DiagCode::Syntax_ExpectedFieldName,
                                 stream.currentLoc(),
                                 "expected a resource field name");
                // Recovery: skip to the next plausible field start or the
                // closing brace. The stop set is the identifiers and the
                // closing brace, at depth 0.
                synchronizeTo(stream,
                              TokenType::IDENTIFIER,
                              TokenType::RBRACE);
                if (!stream.check(TokenType::IDENTIFIER))
                {
                    break; // hit `}`, exit the loop
                }
            }

            ResourceFieldAST *field = parseResourceField(stream, ctx);
            if (field)
            {
                fields.push_back(field);
            }
            else
            {
                // parseResourceField returned nullptr; skip to the next
                // plausible field start or the closing brace.
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
                             "expected '}' to close the resource body");
        }

        ResourceDeclAST *node =
            ctx.arena.make<ResourceDeclAST>(name, fields.build());
        node->loc = startLoc;
        return node;
    }

    ResourceFieldAST *parseResourceField(TokenStream &stream,
                                         ParserContext &ctx)
    {
        // ─── The name ──────────────────────────────────────────────────────
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedFieldName,
                             stream.currentLoc(),
                             "expected a resource field name");
            return nullptr;
        }

        const SourceLocation startLoc = stream.currentLoc();
        const InternedString name = stream.peekValue();
        stream.consume();

        // ─── The type ──────────────────────────────────────────────────────
        if (!stream.match(TokenType::COLON))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedFieldType,
                             stream.currentLoc(),
                             "expected ':' after the field name");
            ResourceFieldAST *node =
                ctx.arena.make<ResourceFieldAST>(name, nullptr, nullptr);
            node->loc = startLoc;
            node->hasSyntaxError = true;
            return node;
        }

        TypeIdAST *type = parseTypeId(stream, ctx);

        // ─── The optional default ──────────────────────────────────────────
        LiteralValueAST *defaultValue = nullptr;
        if (stream.match(TokenType::EQUALS))
        {
            if (!isLiteral(stream.peekType()))
            {
                ctx.diag.errorAt(DiagCode::Syntax_ExpectedFieldDefault,
                                 stream.currentLoc(),
                                 "expected a literal after '='");
                // Fall through: field has no default; the error is
                // reported and the node is marked.
                ResourceFieldAST *node =
                    ctx.arena.make<ResourceFieldAST>(name, type, nullptr);
                node->loc = startLoc;
                node->hasSyntaxError = true;
                return node;
            }
            defaultValue = parseLiteral(stream, ctx);
        }

        ResourceFieldAST *node =
            ctx.arena.make<ResourceFieldAST>(name, type, defaultValue);
        node->loc = startLoc;
        if (type && type->hasSyntaxError)
        {
            node->hasSyntaxError = true;
        }
        return node;
    }

    // =============================================================================
    // Node
    // =============================================================================

    NodeDeclAST *parseNodeDecl(TokenStream &stream, ParserContext &ctx)
    {
        const SourceLocation startLoc = stream.currentLoc();
        stream.consume(); // the `node`

        // ─── The name ──────────────────────────────────────────────────────
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedNodeName,
                             stream.currentLoc(),
                             "expected an identifier after 'node'");
            return nullptr;
        }

        const InternedString name = stream.peekValue();
        stream.consume();

        // ─── The `=` ───────────────────────────────────────────────────────
        if (!stream.match(TokenType::EQUALS))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedNodeExpr,
                             stream.currentLoc(),
                             "expected '=' after the node name");
            return nullptr;
        }

        // ─── The node expression ───────────────────────────────────────────
        NodeExprAST *expr = parseNodeExpr(stream, ctx);

        // ─── The optional `on` clause ──────────────────────────────────────
        ArenaSpan<InternedString> triggers{};
        if (stream.match(TokenType::KW_ON))
        {
            triggers = parseTriggerList(stream, ctx);
        }

        NodeDeclAST *node =
            ctx.arena.make<NodeDeclAST>(name, expr, triggers);
        node->loc = startLoc;
        if (expr && expr->hasSyntaxError)
        {
            node->hasSyntaxError = true;
        }
        return node;
    }

    // =============================================================================
    // Attributes
    // =============================================================================

    ArenaSpan<AttributeAST *> parseAttributeList(TokenStream &stream,
                                                 ParserContext &ctx)
    {
        if (!stream.check(TokenType::AT_SIGN))
        {
            return {};
        }

        auto builder = ctx.arena.makeBuilder<AttributeAST *>();

        while (stream.check(TokenType::AT_SIGN))
        {
            AttributeAST *attr = parseAttribute(stream, ctx);
            if (attr)
            {
                builder.push_back(attr);
            }
        }

        return builder.build();
    }

    AttributeAST *parseAttribute(TokenStream &stream, ParserContext &ctx)
    {
        const SourceLocation startLoc = stream.currentLoc();
        stream.consume(); // the `@`

        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedAttributeName,
                             stream.currentLoc(),
                             "expected an identifier after '@'");
            AttributeAST *node = ctx.arena.make<AttributeAST>();
            node->loc = startLoc;
            node->hasSyntaxError = true;
            return node;
        }

        const InternedString name = stream.peekValue();
        stream.consume();

        AttributeAST *node = ctx.arena.make<AttributeAST>(name);
        node->loc = startLoc;
        return node;
    }

} // namespace lucid::parser