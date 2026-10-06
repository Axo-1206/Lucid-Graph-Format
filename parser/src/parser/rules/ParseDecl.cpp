/// @file parser/src/parser/rules/ParseDecl.cpp
///
/// @brief Implementation of the declaration parsers, the attribute parsers,
///        and the parser's two declaration-adjacent helpers. STUB.
///
/// The real implementations parse:
///
///     import_decl ::= 'import' module_path [ 'as' IDENTIFIER ]
///     enum_decl   ::= 'enum' IDENTIFIER '{' enum_member_list '}'
///
///     resource_decl  ::= attribute_list 'resource' IDENTIFIER
///                            '{' { resource_field } '}'
///     resource_field ::= IDENTIFIER ':' type_id [ '=' literal ]
///
///     node_decl ::= 'node' IDENTIFIER '=' node_expr
///                       [ 'on' trigger_list ]
///
///     attribute_list ::= { '@' IDENTIFIER }
///     module_path    ::= IDENTIFIER { '.' IDENTIFIER }
///     enum_member_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]
///
/// The stubs report NotImplemented and return the minimum valid value
/// for each signature. None of them consumes tokens.
///
/// ─── Attributes are not read here ─────────────────────────────────────────
/// `parseDecl` in Parser.cpp reads the attribute list before dispatching
/// to a specific declaration parser. None of the specific parsers below
/// sees an `@` — that is the design documented in Parser.hpp §Design.

#include "parser/Parser.hpp"

#include "core/diagnostics/DiagCode.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    // =============================================================================
    // Import
    // =============================================================================

    ImportDeclAST *parseImportDecl(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseImportDecl: not yet implemented");
        return nullptr;
    }

    InternedString parseModulePath(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseModulePath: not yet implemented");
        return InternedString{};
    }

    // =============================================================================
    // Enum
    // =============================================================================

    EnumDeclAST *parseEnumDecl(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseEnumDecl: not yet implemented");
        return nullptr;
    }

    ArenaSpan<InternedString> parseEnumMemberList(TokenStream &stream,
                                                  ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseEnumMemberList: not yet implemented");
        return {};
    }

    // =============================================================================
    // Resource
    // =============================================================================

    ResourceDeclAST *parseResourceDecl(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseResourceDecl: not yet implemented");
        return nullptr;
    }

    ResourceFieldAST *parseResourceField(TokenStream &stream,
                                         ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseResourceField: not yet implemented");
        return nullptr;
    }

    // =============================================================================
    // Node
    // =============================================================================

    NodeDeclAST *parseNodeDecl(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseNodeDecl: not yet implemented");
        return nullptr;
    }

    // =============================================================================
    // Attributes
    // =============================================================================

    ArenaSpan<AttributeAST *> parseAttributeList(TokenStream &stream,
                                                 ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseAttributeList: not yet implemented");
        return {};
    }

    AttributeAST *parseAttribute(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseAttribute: not yet implemented");

        AttributeAST *node = ctx.arena.make<AttributeAST>();
        node->hasSyntaxError = true;
        return node;
    }

} // namespace lucid::parser