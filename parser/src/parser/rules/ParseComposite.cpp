/// @file parser/src/parser/rules/ParseComposite.cpp
///
/// @brief Implementation of the composite parser and its field parsers.
///        STUB.
///
/// The real implementations parse:
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
/// The body's declarations are read by dispatching to the same five
/// declaration parsers as the top level, minus `composite_decl`. The
/// composite body is the second (and last) recovery context in the
/// grammar, and the one that shares the `ParseDecl ↔ ParseComposite`
/// mutual reference — resolved by both files including Parser.hpp and
/// neither including the other.
///
/// The stubs report NotImplemented. The composite parser returns
/// nullptr (the "name missing" case). The field parsers return nullptr
/// for the same reason.

#include "parser/Parser.hpp"

#include "core/diagnostics/DiagCode.hpp"

using namespace lucid::diag;

namespace lucid::parser
{

    CompositeDeclAST *parseCompositeDecl(TokenStream &stream, ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseCompositeDecl: not yet implemented");
        return nullptr;
    }

    CompositeInputAST *parseCompositeInput(TokenStream &stream,
                                           ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseCompositeInput: not yet implemented");
        return nullptr;
    }

    CompositeOutputAST *parseCompositeOutput(TokenStream &stream,
                                             ParserContext &ctx)
    {
        ctx.diag.errorAt(DiagCode::Internal_NotImplemented,
                         stream.currentLoc(),
                         "parseCompositeOutput: not yet implemented");
        return nullptr;
    }

} // namespace lucid::parser