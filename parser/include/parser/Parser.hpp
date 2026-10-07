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
 * it. The parser's only validation is syntactic — the shape of what it
 * read against the shape the grammar allows — and its only output on
 * failure is a diagnostic and a node marked `hasSyntaxError`.
 *
 * ─── Design: the two error behaviors ──────────────────────────────────────
 * A parser function that fails does one of two things:
 *
 *   1. PARTIAL-PARSE. It reports a diagnostic and returns a node with
 *      `hasSyntaxError = true`. The node is structurally valid; later
 *      passes skip it. Every function that can partial-parse does so.
 *
 *   2. SKIP. It reports a diagnostic and returns `nullptr`. The caller —
 *      which knows what construct the missing node was supposed to be
 *      part of — runs a synchronizer from ErrorRecovery.hpp and continues
 *      at the next plausible construct start.
 *
 * `nullptr` is a rare return; a function only returns it when it cannot
 * produce even a marked node. Each function's error behavior is
 * documented below.
 *
 * ─── Design: attributes are the dispatcher's job ──────────────────────────
 * A declaration is optionally preceded by a sequence of juxtaposed
 * attributes (`@name`). The declaration parser does not handle them:
 * `parseDecl` reads the attribute sequence before dispatching to the
 * specific declaration parser, and attaches the span to the returned
 * declaration node. A specific parser (parseResourceDecl, parseNodeDecl,
 * ...) never sees an `@`.
 *
 * ─── Design: every parse function has external linkage ────────────────────
 * Every function the parser defines is declared here and defined with
 * external linkage in one of the parser's .cpp files. There are no
 * static functions and no forward declarations in the .cpp files. This
 * keeps every parser function's signature visible in one place.
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

    /// @brief Parse `import module_path [ as IDENTIFIER ]`.
    ///
    /// Resolves nothing. The alias is the alias the source wrote, or the last
    /// path segment if none was written. The path is the dotted form as a
    /// single InternedString ("a.b.c"). The CLI's import-linking step does
    /// the resolution.
    ///
    /// Error behavior: if the module path is missing, reports a diagnostic
    /// and returns nullptr. If the alias after `as` is missing, reports a
    /// diagnostic and uses the last path segment as the alias, returning a
    /// marked node.
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
    /// The body is a sequence of resource fields. Duplicate field names are
    /// not checked here; Sema reports them.
    ///
    /// Error behavior: if the name is missing, returns nullptr. If the body
    /// is malformed, returns a marked node with whatever fields were read.
    ResourceDeclAST *parseResourceDecl(TokenStream &stream, ParserContext &ctx);

    /// @brief Parse one resource field: `name: type [ = literal ]`.
    ///
    /// The default is a literal, per the grammar. It is not a general value.
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

    /// @brief Parse `composite NAME { ... }`.
    ///
    /// The body may contain an `input` block, an `output` block, and a
    /// sequence of body declarations (imports, enums, resources, nodes).
    /// Nested composites are a Sema error, not a parser error; the parser
    /// would accept `composite` inside a body if the grammar allowed it, but
    /// the grammar's `composite_body_decl` does not list `composite_decl`,
    /// so a `composite` keyword inside a body is a syntax error.
    ///
    /// Error behavior: partial-parse. If the name is missing, returns
    /// nullptr. If the body is malformed, returns a marked node with whatever
    /// was read.
    CompositeDeclAST *parseCompositeDecl(TokenStream &stream, ParserContext &ctx);

    // =============================================================================
    // 4. Composite field parsers
    // =============================================================================

    /// @brief Parse one composite input: `name: type`.
    ///
    /// Error behavior: partial-parse. If the type is missing, the returned
    /// node has a null type and is marked. If the name is missing, returns
    /// nullptr.
    CompositeInputAST *parseCompositeInput(TokenStream &stream, ParserContext &ctx);

    /// @brief Parse one composite output: `name: type = value`.
    ///
    /// The right-hand side is a value, not a literal: the grammar's
    /// `composite_output` uses `value`.
    ///
    /// Error behavior: partial-parse. If the type is missing, the node's
    /// type is null and marked. If the value after `=` is missing, the
    /// node's value is null and marked. If the name is missing, returns
    /// nullptr.
    CompositeOutputAST *parseCompositeOutput(TokenStream &stream, ParserContext &ctx);

    // =============================================================================
    // 5. Type parser
    // =============================================================================

    /// @brief Parse a `type_id`: `IDENTIFIER [ '.' IDENTIFIER ]`.
    ///
    /// The name and qualifier are interned. The parser does not resolve the
    /// name against the registry.
    ///
    /// Error behavior: partial-parse. If the identifier is missing, returns
    /// a marked TypeIdAST with an invalid name. Never returns nullptr.
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
    /// Error behavior: partial-parse. If the current token cannot begin a
    /// value, reports a diagnostic and returns a marked UnknownAST. Never
    /// returns nullptr; the caller can rely on a non-null pointer.
    BaseAST *parseValue(TokenStream &stream, ParserContext &ctx);

    /// @brief Parse a literal token into a LiteralValueAST.
    ///
    /// The token's kind (INT_LITERAL, FLOAT_LITERAL, ...) is mapped to a
    /// LiteralKind. The token's payload (already unescaped for strings and
    /// chars) is stored as the literal's text.
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
    /// are a span of values.
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
    // around the list.

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