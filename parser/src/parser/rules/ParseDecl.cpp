/// @file parser/src/parser/rules/ParseDecl.cpp
///
/// @brief The declaration parsers, the attribute parsers, and the two
///        declaration-adjacent helpers.
///
/// ─── The productions ──────────────────────────────────────────────────────
/// The grammar writes:
///
///     import_decl ::= attribute_list 'import' module_path
///     module_path ::= IDENTIFIER { '.' IDENTIFIER }
///
///     enum_decl ::= attribute_list 'enum' IDENTIFIER
///                       '{' enum_member_list '}'
///     enum_member_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]
///
///     resource_decl  ::= attribute_list 'resource' IDENTIFIER
///                            '{' { resource_field } '}'
///     resource_field ::= IDENTIFIER ':' type_id [ '=' value ]
///
///     node_decl ::= attribute_list 'node' IDENTIFIER '=' node_expr
///                       [ 'on' trigger_list ]
///
///     attribute_list ::= { '@' IDENTIFIER }
///
/// ─── Attributes are read by parseDecl, not by these parsers ───────────────
/// `parseDecl` in Parser.cpp reads the attribute list before dispatching.
/// The specific declaration parsers below never see an `@`.
///
/// ─── The import model ─────────────────────────────────────────────────────
/// There is no `as` clause and no alias. An import binds the module's
/// exported declarations bare, and binds the module name (the final path
/// segment) for use as a `::` qualifier in type positions. The parser
/// stores both: the full dotted `path` and the final-segment `name`.
///
/// ─── The shared dispatcher ────────────────────────────────────────────────
/// `parseDeclByKeyword` is the "which parser for which keyword" switch.
/// It is shared between the top-level dispatch. It is declared in
/// ParseDeclInternal.hpp, not Parser.hpp, because it is not part of the
/// parser's public API.

#include "parser/Parser.hpp"
#include "parser/rules/ParseDeclInternal.hpp"

#include "core/diagnostics/DiagCode.hpp"
#include "parser/support/GrammarPositions.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    // =============================================================================
    // Local helpers
    // =============================================================================

    namespace
    {

        /// The final segment of a dotted module path.
        ///
        /// `core.keys`     -> `keys`
        /// `health`        -> `health`
        /// `a.b.c`         -> `c`
        /// ``              -> ``
        ///
        /// The parser uses this to derive an import's module name from its
        /// path. The grammar has no `as` clause, so the module name is
        /// always the final segment.
        InternedString finalPathSegment(StringPool &pool,
                                        InternedString path)
        {
            const std::string_view full = pool.lookupView(path);
            const size_t lastDot = full.rfind('.');
            const std::string_view segment =
                lastDot == std::string_view::npos
                    ? full
                    : full.substr(lastDot + 1);
            return pool.intern(segment);
        }

    } // namespace

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
        default:
            // The caller has already checked canStartTopDecl. Reaching
            // here is a caller bug, not a user error. Report internally
            // and return nullptr so the caller's loop can recover.
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

        // ─── The module name: the final path segment ───────────────────────
        // There is no `as` clause. The module name is always the final
        // segment of the path; the parser computes it here so the AST
        // carries both the full path and the module name.
        const InternedString moduleName = finalPathSegment(ctx.pool, path);

        ImportDeclAST *node =
            ctx.arena.make<ImportDeclAST>(path, moduleName);
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

        ArenaSpan<EnumMemberAST *> members = parseEnumMemberList(stream, ctx);

        EnumDeclAST *node = ctx.arena.make<EnumDeclAST>(name, members);
        node->loc = startLoc;
        return node;
    }

    ArenaSpan<EnumMemberAST *> parseEnumMemberList(TokenStream &stream,
                                                   ParserContext &ctx)
    {
        // The caller has consumed `{`. If the next token is `}`, the
        // list is empty.
        if (stream.check(TokenType::RBRACE))
        {
            stream.consume();
            return {};
        }

        auto builder = ctx.arena.makeBuilder<EnumMemberAST *>();

        // ─── First member ──────────────────────────────────────────────────
        if (!stream.check(TokenType::IDENTIFIER))
        {
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedEnumMember,
                             stream.currentLoc(),
                             "expected an enum member name");
            stream.match(TokenType::RBRACE);
            return builder.build();
        }

        {
            const SourceLocation memberLoc = stream.currentLoc();
            const InternedString name = stream.peekValue();
            stream.consume();

            EnumMemberAST *member =
                ctx.arena.make<EnumMemberAST>(name);
            member->loc = memberLoc;
            builder.push_back(member);
        }

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

            const SourceLocation memberLoc = stream.currentLoc();
            const InternedString name = stream.peekValue();
            stream.consume();

            EnumMemberAST *member =
                ctx.arena.make<EnumMemberAST>(name);
            member->loc = memberLoc;
            builder.push_back(member);
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

        // ─── The field list ────────────────────────────────────────────────
        //
        //     resource_field_list ::= resource_field { ',' resource_field } [ ',' ]
        //
        // An empty list is legal (`resource R { }`); in that case the first
        // token inside the braces is the closing `}`.
        auto fields = ctx.arena.makeBuilder<ResourceFieldAST *>();
        bool bodyHasErrors = false;

        while (!stream.check(TokenType::RBRACE) && !stream.isAtEnd() &&
               ctx.canContinue())
        {
            // ─── One field ────────────────────────────────────────────────
            ResourceFieldAST *field = parseResourceField(stream, ctx);
            if (field)
            {
                if (field->hasSyntaxError)
                    bodyHasErrors = true;
                fields.push_back(field);
            }
            else
            {
                // parseResourceField reported. Skip to the next plausible
                // field start, comma, or the closing brace.
                bodyHasErrors = true;
                synchronizeTo(stream,
                              TokenType::IDENTIFIER,
                              TokenType::COMMA,
                              TokenType::RBRACE);
            }

            // ─── The separator ────────────────────────────────────────────
            // After a field: either `,` (continue) or `}` (end). Anything
            // else is a syntax error.
            if (stream.match(TokenType::COMMA))
            {
                // A trailing comma before `}` is allowed and ends the list.
                if (stream.check(TokenType::RBRACE))
                {
                    break;
                }
                // Otherwise, loop to parse the next field.
                continue;
            }

            if (stream.check(TokenType::RBRACE))
            {
                break;
            }

            // Neither `,` nor `}`. Report and recover.
            bodyHasErrors = true;
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedClosing,
                             stream.currentLoc(),
                             "expected ',' or '}' after a resource field");
            synchronizeTo(stream,
                          TokenType::COMMA,
                          TokenType::RBRACE);
            if (stream.match(TokenType::COMMA))
            {
                if (stream.check(TokenType::RBRACE))
                {
                    break;
                }
                continue;
            }
            break;
        }

        if (!stream.match(TokenType::RBRACE))
        {
            bodyHasErrors = true;
            ctx.diag.errorAt(DiagCode::Syntax_ExpectedClosing,
                             stream.currentLoc(),
                             "expected '}' to close the resource body");
        }

        ResourceDeclAST *node =
            ctx.arena.make<ResourceDeclAST>(name, fields.build());
        node->loc = startLoc;
        if (bodyHasErrors)
            node->hasSyntaxError = true;
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
        // The default is a value (§2.7), not only a literal. The parser
        // accepts any of the four value forms:
        //
        //   - a literal:       `10`, `-7`, `"hello"`, `200.0`
        //   - an identifier:   `some_resource`
        //   - a field access:  `Key.A`, `Config.speed`
        //   - an inline node:  `Float32Node(1.0)`
        //
        // Sema enforces that the default is meaningful for the field's
        // type. The parser does not know the field's type; it accepts the
        // value and lets Sema report a mismatch.
        BaseAST *defaultValue = nullptr;
        if (stream.match(TokenType::EQUALS))
        {
            if (!canStartValue(stream.peekType()))
            {
                ctx.diag.errorAt(DiagCode::Syntax_ExpectedFieldDefault,
                                 stream.currentLoc(),
                                 "expected a value after '='");
            }
            else
            {
                defaultValue = parseValue(stream, ctx);
            }
        }

        ResourceFieldAST *node =
            ctx.arena.make<ResourceFieldAST>(name, type, defaultValue);
        node->loc = startLoc;
        if (type && type->hasSyntaxError)
        {
            node->hasSyntaxError = true;
        }
        if (defaultValue && defaultValue->hasSyntaxError)
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