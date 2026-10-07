/// @file formatter/src/formatter/LayoutBuilder.cpp
///
/// @brief Implementation of the AST → text walker.

#include "LayoutBuilder.hpp"

#include "core/ast/AttributeAST.hpp"
#include "core/ast/DeclAST.hpp"
#include "core/ast/ModuleAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/ast/ValueAST.hpp"

#include <string_view>

namespace lucid::formatter
{

    // =========================================================================
    // Construction and entry point
    // =========================================================================

    LayoutBuilder::LayoutBuilder(const StringPool &pool,
                                 const trivia::TriviaBuffer &trivia,
                                 FormatOptions options)
        : m_pool(pool), m_trivia(trivia), m_writer(options)
    {
    }

    std::string LayoutBuilder::build(const ModuleAST *module)
    {
        if (!module)
        {
            return {};
        }

        writeModule(module);

        // Flush any trivia that follows the last declaration.
        while (m_triviaCursor < m_trivia.size())
        {
            const trivia::Trivia &t = m_trivia[m_triviaCursor];
            writeComment(t);
            m_writer.newline();
            ++m_triviaCursor;
        }

        return m_writer.takeStr();
    }

    // =========================================================================
    // Helpers
    // =========================================================================

    std::string_view LayoutBuilder::lookup(InternedString s) const
    {
        return m_pool.lookupView(s);
    }

    void LayoutBuilder::writeStringLiteral(std::string_view text)
    {
        m_writer.write('"');
        for (char c : text)
        {
            switch (c)
            {
            case '"':
                m_writer.write("\\\"");
                break;
            case '\\':
                m_writer.write("\\\\");
                break;
            case '\n':
                m_writer.write("\\n");
                break;
            case '\t':
                m_writer.write("\\t");
                break;
            case '\r':
                m_writer.write("\\r");
                break;
            case '\0':
                m_writer.write("\\0");
                break;
            default:
                m_writer.write(c);
                break;
            }
        }
        m_writer.write('"');
    }

    void LayoutBuilder::writeCharLiteral(std::string_view text)
    {
        m_writer.write('\'');
        for (char c : text)
        {
            switch (c)
            {
            case '\'':
                m_writer.write("\\'");
                break;
            case '\\':
                m_writer.write("\\\\");
                break;
            case '\n':
                m_writer.write("\\n");
                break;
            case '\t':
                m_writer.write("\\t");
                break;
            case '\r':
                m_writer.write("\\r");
                break;
            case '\0':
                m_writer.write("\\0");
                break;
            default:
                m_writer.write(c);
                break;
            }
        }
        m_writer.write('\'');
    }

    // =========================================================================
    // Trivia
    // =========================================================================

    void LayoutBuilder::writeComment(const trivia::Trivia &t)
    {
        if (t.kind == trivia::TriviaKind::LineComment)
        {
            m_writer.write("--");
            m_writer.write(lookup(t.text));
        }
        else
        {
            m_writer.write("/-");
            m_writer.write(lookup(t.text));
            m_writer.write("-/");
        }
    }

    void LayoutBuilder::drainCommentsBefore(SourceLocation loc)
    {
        while (m_triviaCursor < m_trivia.size())
        {
            const trivia::Trivia &t = m_trivia[m_triviaCursor];
            if (t.loc.value >= loc.value)
                break;

            if (m_writer.column() > 0)
            {
                m_writer.newline();
            }
            writeComment(t);
            m_writer.newline();
            ++m_triviaCursor;
        }
    }

    void LayoutBuilder::drainTrailingComments(uint32_t line)
    {
        while (m_triviaCursor < m_trivia.size())
        {
            const trivia::Trivia &t = m_trivia[m_triviaCursor];
            if (t.loc.line() != line)
                break;

            m_writer.write("  ");
            writeComment(t);
            ++m_triviaCursor;
        }
    }

    // =========================================================================
    // Node dispatch
    // =========================================================================

    void LayoutBuilder::writeNode(const BaseAST *node)
    {
        if (!node)
            return;

        drainCommentsBefore(node->loc);

        switch (node->kind)
        {
        case ASTKind::TypeId:
            writeTypeId(node->as<TypeIdAST>());
            break;
        case ASTKind::NodeExpr:
            writeNodeExpr(node->as<NodeExprAST>());
            break;
        case ASTKind::LiteralValue:
            writeLiteralValue(node->as<LiteralValueAST>());
            break;
        case ASTKind::IdentifierValue:
            writeIdentifierValue(node->as<IdentifierValueAST>());
            break;
        case ASTKind::FieldAccessValue:
            writeFieldAccessValue(node->as<FieldAccessValueAST>());
            break;
        case ASTKind::InlineNodeValue:
            writeInlineNodeValue(node->as<InlineNodeValueAST>());
            break;
        case ASTKind::ImportDecl:
            writeImportDecl(node->as<ImportDeclAST>());
            break;
        case ASTKind::EnumDecl:
            writeEnumDecl(node->as<EnumDeclAST>());
            break;
        case ASTKind::ResourceDecl:
            writeResourceDecl(node->as<ResourceDeclAST>());
            break;
        case ASTKind::ResourceField:
            writeResourceField(node->as<ResourceFieldAST>());
            break;
        case ASTKind::NodeDecl:
            writeNodeDecl(node->as<NodeDeclAST>());
            break;
        case ASTKind::CompositeDecl:
            writeCompositeDecl(node->as<CompositeDeclAST>());
            break;
        default:
            break;
        }
    }

    // =========================================================================
    // Module
    // =========================================================================

    void LayoutBuilder::writeModule(const ModuleAST *node)
    {
        bool first = true;
        for (DeclAST *decl : node->decls)
        {
            if (!decl)
                continue;

            if (decl->kind == ASTKind::Unknown)
            {
                drainCommentsBefore(decl->loc);
                continue;
            }

            if (!first)
            {
                m_writer.newline(); // blank line between declarations
            }

            writeDecl(decl);
            first = false;
        }
    }

    // =========================================================================
    // Declarations
    // =========================================================================

    void LayoutBuilder::writeDecl(const DeclAST *node)
    {
        if (!node)
            return;

        drainCommentsBefore(node->loc);

        switch (node->kind)
        {
        case ASTKind::ImportDecl:
            writeImportDecl(node->as<ImportDeclAST>());
            break;
        case ASTKind::EnumDecl:
            writeEnumDecl(node->as<EnumDeclAST>());
            break;
        case ASTKind::ResourceDecl:
            writeResourceDecl(node->as<ResourceDeclAST>());
            break;
        case ASTKind::NodeDecl:
            writeNodeDecl(node->as<NodeDeclAST>());
            break;
        case ASTKind::CompositeDecl:
            writeCompositeDecl(node->as<CompositeDeclAST>());
            break;
        default:
            break;
        }
    }

    void LayoutBuilder::writeAttributes(ArenaSpan<AttributeAST *> attrs)
    {
        for (AttributeAST *attr : attrs)
        {
            if (!attr)
                continue;

            drainCommentsBefore(attr->loc);

            m_writer.write('@');
            m_writer.write(lookup(attr->name));
            m_writer.newline();
        }
    }

    void LayoutBuilder::writeImportDecl(const ImportDeclAST *node)
    {
        writeAttributes(node->attributes);

        m_writer.write("import ");
        m_writer.write(lookup(node->path));

        const std::string_view path = lookup(node->path);
        const size_t lastDot = path.rfind('.');
        const std::string_view lastSegment =
            lastDot == std::string_view::npos ? path : path.substr(lastDot + 1);

        if (lookup(node->name) != lastSegment)
        {
            m_writer.write(" as ");
            m_writer.write(lookup(node->name));
        }

        drainTrailingComments(node->loc.line());
        m_writer.newline();
    }

    void LayoutBuilder::writeEnumDecl(const EnumDeclAST *node)
    {
        writeAttributes(node->attributes);

        m_writer.write("enum ");
        m_writer.write(lookup(node->name));
        m_writer.write(" {");
        m_writer.newline();
        m_writer.indent();

        for (InternedString member : node->members)
        {
            m_writer.write(lookup(member));
            m_writer.newline();
        }

        m_writer.dedent();
        m_writer.write("}");
        m_writer.newline();
    }

    void LayoutBuilder::writeResourceDecl(const ResourceDeclAST *node)
    {
        writeAttributes(node->attributes);

        m_writer.write("resource ");
        m_writer.write(lookup(node->name));
        m_writer.write(" {");
        m_writer.newline();
        m_writer.indent();

        for (ResourceFieldAST *field : node->fields)
        {
            if (!field)
                continue;
            writeResourceField(field);
        }

        m_writer.dedent();
        m_writer.write("}");
        m_writer.newline();
    }

    void LayoutBuilder::writeResourceField(const ResourceFieldAST *node)
    {
        drainCommentsBefore(node->loc);

        m_writer.write(lookup(node->name));
        m_writer.write(": ");

        if (node->type)
        {
            writeTypeId(node->type);
        }

        if (node->defaultValue)
        {
            m_writer.write(" = ");
            writeLiteralValue(node->defaultValue);
        }

        drainTrailingComments(node->loc.line());
        m_writer.newline();
    }

    void LayoutBuilder::writeNodeDecl(const NodeDeclAST *node)
    {
        writeAttributes(node->attributes);

        m_writer.write("node ");
        m_writer.write(lookup(node->name));
        m_writer.write(" = ");

        if (node->expr)
        {
            writeNodeExpr(node->expr);
        }

        if (!node->triggers.empty())
        {
            m_writer.write(" on ");
            for (size_t i = 0; i < node->triggers.size(); ++i)
            {
                if (i > 0)
                    m_writer.write(", ");
                m_writer.write(lookup(node->triggers[i]));
            }
        }

        drainTrailingComments(node->loc.line());
        m_writer.newline();
    }

    void LayoutBuilder::writeCompositeInput(const CompositeInputAST *node)
    {
        drainCommentsBefore(node->loc);

        m_writer.write(lookup(node->name));
        m_writer.write(": ");
        if (node->type)
        {
            writeTypeId(node->type);
        }

        drainTrailingComments(node->loc.line());
        m_writer.newline();
    }

    void LayoutBuilder::writeCompositeOutput(const CompositeOutputAST *node)
    {
        drainCommentsBefore(node->loc);

        m_writer.write(lookup(node->name));
        m_writer.write(": ");
        if (node->type)
        {
            writeTypeId(node->type);
        }
        if (node->value)
        {
            m_writer.write(" = ");
            writeValue(node->value);
        }

        drainTrailingComments(node->loc.line());
        m_writer.newline();
    }

    void LayoutBuilder::writeCompositeDecl(const CompositeDeclAST *node)
    {
        writeAttributes(node->attributes);

        m_writer.write("composite ");
        m_writer.write(lookup(node->name));
        m_writer.write(" {");
        m_writer.newline();

        bool wroteAnySection = false;

        if (!node->inputs.empty())
        {
            m_writer.indent();
            m_writer.write("input {");
            m_writer.newline();
            m_writer.indent();
            for (CompositeInputAST *input : node->inputs)
            {
                if (!input)
                    continue;
                writeCompositeInput(input);
            }
            m_writer.dedent();
            m_writer.write("}");
            m_writer.newline();
            m_writer.dedent();
            wroteAnySection = true;
        }

        if (!node->outputs.empty())
        {
            if (wroteAnySection)
                m_writer.newline();

            m_writer.indent();
            m_writer.write("output {");
            m_writer.newline();
            m_writer.indent();
            for (CompositeOutputAST *output : node->outputs)
            {
                if (!output)
                    continue;
                writeCompositeOutput(output);
            }
            m_writer.dedent();
            m_writer.write("}");
            m_writer.newline();
            m_writer.dedent();
            wroteAnySection = true;
        }

        if (!node->body.empty())
        {
            if (wroteAnySection)
                m_writer.newline();

            m_writer.indent();
            bool first = true;
            for (DeclAST *decl : node->body)
            {
                if (!decl)
                    continue;
                if (decl->kind == ASTKind::Unknown)
                {
                    drainCommentsBefore(decl->loc);
                    continue;
                }
                if (!first)
                    m_writer.newline();
                writeDecl(decl);
                first = false;
            }
            m_writer.dedent();
        }

        m_writer.write("}");
        m_writer.newline();
    }

    // =========================================================================
    // Types
    // =========================================================================

    void LayoutBuilder::writeTypeId(const TypeIdAST *node)
    {
        if (!node)
            return;
        if (node->isQualified())
        {
            m_writer.write(lookup(node->qualifier));
            m_writer.write('.');
        }
        m_writer.write(lookup(node->name));
    }

    // =========================================================================
    // Values
    // =========================================================================

    void LayoutBuilder::writeValue(const BaseAST *node)
    {
        if (!node)
            return;
        switch (node->kind)
        {
        case ASTKind::LiteralValue:
            writeLiteralValue(node->as<LiteralValueAST>());
            break;
        case ASTKind::IdentifierValue:
            writeIdentifierValue(node->as<IdentifierValueAST>());
            break;
        case ASTKind::FieldAccessValue:
            writeFieldAccessValue(node->as<FieldAccessValueAST>());
            break;
        case ASTKind::InlineNodeValue:
            writeInlineNodeValue(node->as<InlineNodeValueAST>());
            break;
        default:
            break;
        }
    }

    void LayoutBuilder::writeLiteralValue(const LiteralValueAST *node)
    {
        if (!node)
            return;
        const std::string_view text = lookup(node->text);
        switch (node->kind)
        {
        case LiteralKind::String:
            writeStringLiteral(text);
            break;
        case LiteralKind::Char:
            writeCharLiteral(text);
            break;
        case LiteralKind::Int:
        case LiteralKind::Float:
        case LiteralKind::Bool:
        case LiteralKind::Nil:
            m_writer.write(text);
            break;
        }
    }

    void LayoutBuilder::writeIdentifierValue(const IdentifierValueAST *node)
    {
        if (!node)
            return;
        m_writer.write(lookup(node->name));
    }

    void LayoutBuilder::writeFieldAccessValue(const FieldAccessValueAST *node)
    {
        if (!node)
            return;
        m_writer.write(lookup(node->object));
        m_writer.write('.');
        m_writer.write(lookup(node->field));
    }

    void LayoutBuilder::writeInlineNodeValue(const InlineNodeValueAST *node)
    {
        if (!node)
            return;
        if (node->node)
            writeNodeExpr(node->node);
    }

    void LayoutBuilder::writeNodeExpr(const NodeExprAST *node)
    {
        if (!node)
            return;
        if (node->type)
            writeTypeId(node->type);
        m_writer.write('(');
        for (size_t i = 0; i < node->args.size(); ++i)
        {
            if (i > 0)
                m_writer.write(", ");
            writeValue(node->args[i]);
        }
        m_writer.write(')');
    }

} // namespace lucid::formatter