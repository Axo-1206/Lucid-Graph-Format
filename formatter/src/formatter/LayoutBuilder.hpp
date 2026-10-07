/// @file formatter/src/formatter/LayoutBuilder.hpp
///
/// @brief The AST → text walker.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// The class that does the actual formatting. It walks a ModuleAST in
/// pre-order, emits text through an IndentWriter, and interleaves
/// comments from a TriviaBuffer.
///
/// ─── This header is internal ──────────────────────────────────────────────
/// It lives under `formatter/src`, not `formatter/include`. It is not
/// installed and is not part of the public API. Only Formatter.cpp
/// includes it.
///
/// ─── Layout: Policy A ─────────────────────────────────────────────────────
/// Every construct is emitted on a single line, except for the ones
/// whose grammar mandates a block: enums, resources, composites. Node
/// expressions and their argument lists always stay on one line,
/// regardless of length. There is no line-length threshold and no
/// line breaking.
///
/// The canonical layout is documented in the .cpp.

#pragma once

#include "core/ast/ModuleAST.hpp"
#include "core/memory/StringPool.hpp"
#include "core/trivia/TriviaBuffer.hpp"
#include "formatter/FormatOptions.hpp"
#include "formatter/IndentWriter.hpp"

#include <cstdint>
#include <string>

namespace lucid::formatter
{

    class LayoutBuilder
    {
    public:
        LayoutBuilder(const StringPool &pool,
                      const trivia::TriviaBuffer &trivia,
                      FormatOptions options);

        /// Walk the module and produce the formatted text. A null module
        /// produces an empty string.
        std::string build(const ModuleAST *module);

    private:
        // ─── Trivia ────────────────────────────────────────────────────────

        /// Drain trivia whose location is strictly before `loc`. A
        /// comment on the same line as the last emitted node is emitted
        /// as a trailing comment on the current line; all other trivia
        /// is emitted as a leading comment on its own line at the
        /// current indent.
        ///
        /// Called by `writeNode` before dispatching. Also called at the
        /// end of the module to flush any remaining trivia.
        void drainCommentsBefore(SourceLocation loc);

        /// Emit one comment. The caller is responsible for the line
        /// position and any leading whitespace.
        void writeComment(const trivia::Trivia &t);

        /// Drain all trivia whose location is on `line` and emit them as
        /// trailing comments on the current line. Called by every
        /// one-line emit function before its final newline.
        void drainTrailingComments(uint32_t line);

        // ─── Node dispatch ─────────────────────────────────────────────────

        void writeNode(const BaseAST *node);

        // ─── Module ────────────────────────────────────────────────────────

        void writeModule(const ModuleAST *node);

        // ─── Declarations ──────────────────────────────────────────────────

        void writeDecl(const DeclAST *node);
        void writeImportDecl(const ImportDeclAST *node);
        void writeEnumDecl(const EnumDeclAST *node);
        void writeResourceDecl(const ResourceDeclAST *node);
        void writeResourceField(const ResourceFieldAST *node);
        void writeNodeDecl(const NodeDeclAST *node);
        void writeCompositeDecl(const CompositeDeclAST *node);
        void writeCompositeInput(const CompositeInputAST *node);
        void writeCompositeOutput(const CompositeOutputAST *node);

        // ─── Attributes ────────────────────────────────────────────────────

        void writeAttributes(ArenaSpan<AttributeAST *> attrs);

        // ─── Types ─────────────────────────────────────────────────────────

        void writeTypeId(const TypeIdAST *node);

        // ─── Values ────────────────────────────────────────────────────────

        void writeValue(const BaseAST *node);
        void writeLiteralValue(const LiteralValueAST *node);
        void writeIdentifierValue(const IdentifierValueAST *node);
        void writeFieldAccessValue(const FieldAccessValueAST *node);
        void writeInlineNodeValue(const InlineNodeValueAST *node);
        void writeNodeExpr(const NodeExprAST *node);

        // ─── Helpers ───────────────────────────────────────────────────────

        std::string_view lookup(InternedString s) const;
        void writeStringLiteral(std::string_view text);
        void writeCharLiteral(std::string_view text);

        // ─── State ─────────────────────────────────────────────────────────

        const StringPool &m_pool;
        const trivia::TriviaBuffer &m_trivia;
        IndentWriter m_writer;
        size_t m_triviaCursor = 0;
    };

} // namespace lucid::formatter