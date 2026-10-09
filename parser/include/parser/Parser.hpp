/**
 * @file parser/Parser.hpp
 * @brief The Lucid parser: one file in, one ModuleAST out.
 *
 * ─── Design: one file per call ────────────────────────────────────────────
 * The parser does not walk imports, does not resolve module paths, and
 * does not touch the filesystem. Module resolution is the CLI's job. The
 * parser's inputs are one file's path, one file's source, and a
 * ParserContext; its output is one ModuleAST*.
 *
 * ─── Design: parse functions build AST; they do not analyze ───────────────
 * No parser function resolves a name, checks a type, or decides whether
 * the program is well-formed. The parser produces a tree; Sema validates
 * it.
 *
 * ─── Design: the two error behaviors ──────────────────────────────────────
 * A parser function that fails does one of two things:
 *
 *   1. PARTIAL-PARSE. It reports a diagnostic and returns a node with
 *      `hasSyntaxError = true`.
 *
 *   2. SKIP. It reports a diagnostic and returns `nullptr`. The caller
 *      runs a synchronizer from ErrorRecovery.hpp and continues at the
 *      next plausible construct start.
 *
 * ─── Design: attributes are the dispatcher's job ──────────────────────────
 * A declaration is optionally preceded by a sequence of juxtaposed
 * attributes (`@name`). The grammar allows an attribute list on any of
 * the four top-level declarations. `parseDecl` reads the attribute
 * sequence before dispatching to the specific declaration parser.
 *
 * ─── Design: every parse function has external linkage ────────────────────
 * Every function the parser defines is declared here and defined with
 * external linkage in one of the parser's .cpp files.
 */

#pragma once

#include "core/Tokens.hpp"
#include "core/ast/BaseAST.hpp"
#include "core/ast/DeclAST.hpp"
#include "core/ast/ModuleAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/ast/ValueAST.hpp"
#include "core/ast/AttributeAST.hpp"
#include "parser/context/ParserContext.hpp"
#include "parser/context/TokenStream.hpp"
#include "parser/support/ErrorRecovery.hpp"

#include <string_view>
#include <vector>

namespace lucid::parser
{

    // =============================================================================
    // 1. Entry point
    // =============================================================================

    /// @brief Parse one source file into a ModuleAST.
    ///
    /// This is the parser's only public entry point. It lexes the source,
    /// constructs a TokenStream, parses every top-level declaration, and
    /// returns the ModuleAST. It does not resolve imports, does not read
    /// other files, and does not walk dependencies.
    ///
    /// The returned pointer is never null. A file that could not be parsed
    /// (lexer error, unrecoverable syntax error) still produces a real
    /// ModuleAST with `hasErrors == true`. Callers detect failure by
    /// checking `module->hasErrors`, not by checking for null.
    ///
    /// The parser tags every diagnostic it raises with the file path, using
    /// a ScopedDiagnosticFile guard for the duration of the parse.
    ///
    /// Preconditions:
    ///   - `ctx` was constructed with a StringPool, an ASTArena, a
    ///     DiagnosticEngine, and (temporarily) a TokenStream. The temporary
    ///     stream is discarded; parseFile tokenizes `source` into its own.
    ///
    /// Postconditions:
    ///   - The returned ModuleAST is arena-allocated and lives as long as
    ///     the arena.
    ///   - `ctx.diag` contains the diagnostics raised during the parse.
    ///   - `module->hasErrors` is true iff `ctx.diag.hasErrors()` was true at
    ///     the end of the parse.
    ModuleAST *parseFile(std::string_view path,
                         std::string_view source,
                         ParserContext &ctx);

    // =============================================================================
    // 2. Dispatchers
    // =============================================================================

    /// @brief Parse one top-level declaration.
    ///
    /// Reads an optional attribute sequence, then dispatches on the
    /// declaration keyword that follows. The returned node's `loc` is the
    /// location of the first attribute, or of the declaration keyword if
    /// there were none.
    ///
    /// The grammar allows an attribute list on any of the four top-level
    /// declarations:
    ///
    ///     import_decl   ::= attribute_list 'import'   ...
    ///     enum_decl     ::= attribute_list 'enum'     ...
    ///     resource_decl ::= attribute_list 'resource' ...
    ///     node_decl     ::= attribute_list 'node'     ...
    ///
    /// The parser does not check which attribute names are recognized or
    /// which declarations they may appear on. Sema does both.
    ///
    /// Error behavior: if the attribute sequence is followed by a token that
    /// is not a declaration keyword, reports "expected a declaration after
    /// the attribute(s)" and returns a marked UnknownAST (as a DeclAST is not
    /// constructible from UnknownAST). Concretely, this function returns
    /// `nullptr` in that case and the caller's top-level loop runs a
    /// synchronizer.
    ///
    /// Returns nullptr when the current token cannot begin a declaration.
    DeclAST *parseDecl(TokenStream &stream, ParserContext &ctx);

    // =============================================================================
    // 3. Declaration parsers
    // =============================================================================
    //
    // Each is called with the cursor on its keyword; attributes have already
    // been read by parseDecl.

    /// @brief Parse `import module_path`.
    ///
    /// Resolves nothing. The module name is the final path segment, and is
    /// stored in the declaration's `name` field. The path is the dotted
    /// form as a single InternedString ("a.b.c"). The CLI's import-linking
    /// step does the resolution.
    ///
    /// There is no `as` clause and no alias. The grammar binds the module's
    /// exported declarations bare, and binds the module name (the final
    /// path segment) for use as a `::` qualifier in type positions.
    ///
    /// Error behavior: if the module path is missing, reports a diagnostic
    /// and returns nullptr. If a segment after a `.` is missing, reports a
    /// diagnostic and returns a marked node with the path read so far and
    /// the last valid segment as the module name.
    ImportDeclAST *parseImportDecl(TokenStream &stream, ParserContext &ctx);

    /// @brief Parse `enum NAME { members }`.
    ///
    /// The members are interned identifiers. Duplicate members are not
    /// checked here; Sema reports them.
    ///
    /// Error behavior: if the name is missing, reports and returns nullptr.
    /// If the body is malformed, returns a marked node with whatever members
    /// were read.
    EnumDeclAST *parseEnumDecl(TokenStream &stream, ParserContext &ctx);

    /// @brief Parse `resource NAME { fields }`.
    ///
    /// The body is a comma-separated list of resource fields
    /// (`name: type [ = value ]`), with an optional trailing comma. The
    /// list is structurally identical to an enum's member list. Duplicate
    /// field names are not checked here; Sema reports them.
    ///
    /// Error behavior: if the name is missing, returns nullptr. If the body
    /// is malformed, returns a marked node with whatever fields were read.
    ResourceDeclAST *parseResourceDecl(TokenStream &stream, ParserContext &ctx);

    /// @brief Parse one resource field: `name: type [ = value ]`.
    ///
    /// The default is a `value` (grammar §2.7), not only a literal. All four
    /// value forms are accepted:
    ///
    ///   - a literal: `10`, `-7`, `"hello"`, `200.0`
    ///   - an identifier: `some_resource`
    ///   - a field access: `Key.A`, `Config.speed`
    ///   - an inline node: `Float32Node(1.0)`
    ///
    /// The grammar allows any value; Sema enforces that the default is
    /// meaningful for the field's type. A default of `Key.A` requires the
    /// field's type to be an enum or a compatible type; the parser does not
    /// check this.
    ///
    /// Error behavior: partial-parse. If the type is missing, the returned
    /// field has a null type and is marked. If the default is missing after
    /// `=`, the returned field has a null default and is marked. If the name
    /// is missing, returns nullptr.
    ResourceFieldAST *parseResourceField(TokenStream &stream, ParserContext &ctx);

    /// @brief Parse `node NAME = node_expr [ on trigger_list ]`.
    ///
    /// The trigger list is a span of names; it is empty when there is no
    /// `on` clause.
    ///
    /// Error behavior: partial-parse. If the right-hand side is missing,
    /// returns a marked node with a null `expr`. If the name is missing,
    /// returns nullptr.
    NodeDeclAST *parseNodeDecl(TokenStream &stream, ParserContext &ctx);

    // =============================================================================
    // 5. Type parser
    // =============================================================================

    /// @brief Parse a `type_id`: `IDENTIFIER [ '::' IDENTIFIER ]`.
    ///
    /// The name and qualifier are interned. The parser does not resolve the
    /// name against the registry. A qualified type is written with `::`
    /// (`core::Key`); the single `:` is the resource-field separator and
    /// does not appear inside a `type_id`.
    ///
    /// Error behavior: partial-parse. If the identifier is missing, returns
    /// a marked TypeIdAST with an invalid name. If the identifier after `::`
    /// is missing, returns a marked TypeIdAST with the qualifier set and an
    /// invalid name. Never returns nullptr.
    TypeIdAST *parseTypeId(TokenStream &stream, ParserContext &ctx);

    // =============================================================================
    // 6. Value parsers
    // =============================================================================

    /// @brief Parse a value: literal, identifier, field access, or inline node.
    ///
    /// The four forms are distinguished by the first token:
    ///   - a literal token → LiteralValueAST
    ///   - IDENTIFIER followed by `.` → FieldAccessValueAST
    ///   - IDENTIFIER followed by `(` → InlineNodeValueAST
    ///   - IDENTIFIER alone → IdentifierValueAST
    ///
    /// A signed literal (`-7`, `+3.14`) is a single literal token; the sign
    /// is part of the token's text, not a separate token. There is no
    /// unary-minus production.
    ///
    /// Error behavior: partial-parse. If the current token cannot begin a
    /// value, reports a diagnostic and returns a marked UnknownAST. Never
    /// returns nullptr; the caller can rely on a non-null pointer.
    BaseAST *parseValue(TokenStream &stream, ParserContext &ctx);

    /// @brief Parse a literal token into a LiteralValueAST.
    ///
    /// The token's kind (INT_LITERAL, FLOAT_LITERAL, ...) is mapped to a
    /// LiteralKind. The token's payload (already unescaped for strings and
    /// chars, and including any leading sign for numbers) is stored as the
    /// literal's text.
    ///
    /// Error behavior: partial-parse. If the current token is not a literal,
    /// reports a diagnostic and returns a marked literal with kind Int and
    /// an invalid text. Never returns nullptr.
    LiteralValueAST *parseLiteral(TokenStream &stream, ParserContext &ctx);

    // =============================================================================
    // 7. Node expression parsers
    // =============================================================================

    /// @brief Parse a node expression: `NodeType '(' [ arg_list ] ')'`.
    ///
    /// The NodeType is a TypeIdAST (same shape as `type_id`). The arguments
    /// are a span of values. A qualified node type uses `::` (`physics::Body`).
    ///
    /// Error behavior: partial-parse. If the node type is missing, the
    /// returned node has a null type. If the argument list is malformed, the
    /// returned node has the arguments that were read and is marked. Never
    /// returns nullptr.
    NodeExprAST *parseNodeExpr(TokenStream &stream, ParserContext &ctx);

    /// @brief Parse a trigger list: `IDENTIFIER { ',' IDENTIFIER } [ ',' ]`.
    ///
    /// Returns a span of trigger names. The caller has established that the
    /// current token is `on` and consumed it.
    ///
    /// Error behavior: partial-parse. If the first trigger name is missing,
    /// reports a diagnostic and returns an empty span. The caller marks the
    /// enclosing node.
    ArenaSpan<InternedString> parseTriggerList(TokenStream &stream,
                                               ParserContext &ctx);

    /// @brief Parse a comma-separated argument list inside `( ... )`.
    ///
    /// The caller has consumed the opening `(`. This function reads the
    /// arguments (if any) and consumes the closing `)`.
    ///
    /// Error behavior: partial-parse. If the closing `)` is missing, reports
    /// a diagnostic and returns the arguments that were read.
    ArenaSpan<BaseAST *> parseArgList(TokenStream &stream, ParserContext &ctx);

    // =============================================================================
    // 8. Attribute parsers
    // =============================================================================
    //
    // Attributes are juxtaposed: a declaration may be preceded by any number
    // of `@name` prefixes, with no separator between them and no brackets
    // around the list. The grammar allows an attribute list on any of the
    // four top-level declarations.

    /// @brief Parse a sequence of `@name` attributes.
    ///
    /// If the current token is not `@`, returns an empty span and consumes
    /// nothing. Otherwise reads attributes until the next non-`@` token.
    ///
    /// Error behavior: partial-parse. If an attribute's name is missing,
    /// reports a diagnostic and produces a marked AttributeAST with an
    /// invalid name. The sequence continues with the next `@` if any.
    ArenaSpan<AttributeAST *> parseAttributeList(TokenStream &stream,
                                                 ParserContext &ctx);

    /// @brief Parse one attribute: `@` followed by an identifier.
    ///
    /// The caller has NOT consumed the `@`; this function consumes it and
    /// the identifier.
    ///
    /// Error behavior: partial-parse. If the identifier after `@` is
    /// missing, reports a diagnostic and returns a marked AttributeAST with
    /// an invalid name. Never returns nullptr.
    AttributeAST *parseAttribute(TokenStream &stream, ParserContext &ctx);

    // =============================================================================
    // 9. Helpers
    // =============================================================================

    /// @brief Parse a dotted import path: `a`, `a.b`, `a.b.c`.
    ///
    /// Returns the path as a single InternedString with the segments joined
    /// by '.'. The caller has consumed `import`.
    ///
    /// The `.` here is a module-path separator, not field access and not the
    /// `::` qualifier. A module path is always dotted; the module name (the
    /// final segment) is used later as a `::` qualifier.
    ///
    /// Error behavior: partial-parse. If the first segment is missing,
    /// reports a diagnostic and returns an invalid InternedString. If a
    /// segment after a `.` is missing, reports and returns the path read so
    /// far.
    InternedString parseModulePath(TokenStream &stream, ParserContext &ctx);

    /// @brief Parse an enum member list: `IDENTIFIER { ',' IDENTIFIER } [ ',' ]`.
    ///
    /// The caller has consumed the opening `{`. This function reads the
    /// members (if any) and consumes the closing `}`.
    ///
    /// Error behavior: partial-parse. If a member is missing between commas,
    /// reports a diagnostic and skips it. If the closing `}` is missing,
    /// reports and returns the members that were read.
    ArenaSpan<EnumMemberAST *> parseEnumMemberList(TokenStream &stream,
                                                   ParserContext &ctx);

} // namespace lucid::parser