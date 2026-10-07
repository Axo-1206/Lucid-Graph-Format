/// @file parser/src/parser/dump/JSONDumper.cpp
///
/// @brief Implementation of the AST → JSON dumper.
///
/// ─── Structure ────────────────────────────────────────────────────────────
/// One internal class, AstDumper, holds a JSONWriter and a StringPool
/// reference. The public function dumpModule constructs one, runs it,
/// and returns the writer's string.
///
/// The dumper walks the AST with a single recursive function,
/// writeNode, that switches on the node's ASTKind. Each case writes the
/// node's fields and recurses into its children.
///
/// ─── The ASTKind → string map ─────────────────────────────────────────────
/// astKindName is a local function that maps each ASTKind enumerator to
/// its string name. The names match the enum's identifiers. The function
/// has no default case: a new ASTKind that is not handled produces a
/// compile warning (with -Wswitch), which is what we want.

#include "parser/dump/JSONDumper.hpp"

#include "core/ast/AttributeAST.hpp"
#include "core/ast/DeclAST.hpp"
#include "core/ast/ModuleAST.hpp"
#include "core/ast/TypeAST.hpp"
#include "core/ast/ValueAST.hpp"
#include "parser/dump/JSONWriter.hpp"

#include <string_view>

namespace lucid::parser::dump
{

    namespace
    {

        // ─── ASTKind → string ─────────────────────────────────────────────

        const char *astKindName(ASTKind kind)
        {
            switch (kind)
            {
            case ASTKind::Attribute:
                return "Attribute";
            case ASTKind::TypeId:
                return "TypeId";
            case ASTKind::NodeExpr:
                return "NodeExpr";
            case ASTKind::LiteralValue:
                return "LiteralValue";
            case ASTKind::IdentifierValue:
                return "IdentifierValue";
            case ASTKind::FieldAccessValue:
                return "FieldAccessValue";
            case ASTKind::InlineNodeValue:
                return "InlineNodeValue";
            case ASTKind::Decl:
                return "Decl";
            case ASTKind::ImportDecl:
                return "ImportDecl";
            case ASTKind::EnumDecl:
                return "EnumDecl";
            case ASTKind::ResourceDecl:
                return "ResourceDecl";
            case ASTKind::ResourceField:
                return "ResourceField";
            case ASTKind::NodeDecl:
                return "NodeDecl";
            case ASTKind::CompositeDecl:
                return "CompositeDecl";
            case ASTKind::CompositeInput:
                return "CompositeInput";
            case ASTKind::CompositeOutput:
                return "CompositeOutput";
            case ASTKind::Module:
                return "Module";
            case ASTKind::Unknown:
                return "Unknown";
            }
            return "Unknown";
        }

        const char *literalKindName(LiteralKind k)
        {
            switch (k)
            {
            case LiteralKind::Int:
                return "Int";
            case LiteralKind::Float:
                return "Float";
            case LiteralKind::String:
                return "String";
            case LiteralKind::Char:
                return "Char";
            case LiteralKind::Bool:
                return "Bool";
            case LiteralKind::Nil:
                return "Nil";
            }
            return "Nil";
        }

    } // namespace

    // ─── AstDumper ───────────────────────────────────────────────────────────

    class AstDumper
    {
    public:
        explicit AstDumper(const StringPool &pool) : m_pool(pool) {}

        std::string dump(const ModuleAST *module)
        {
            writeNode(module);
            return m_writer.str();
        }

    private:
        // ─── Helpers ───────────────────────────────────────────────────────

        std::string_view lookup(InternedString s)
        {
            return m_pool.lookupView(s);
        }

        /// Write the common fields every node has: kind, loc, and
        /// hasSyntaxError (only when true).
        void writeNodeHeader(const BaseAST *node)
        {
            m_writer.kv("kind", astKindName(node->kind));
            if (node->loc.isKnown())
            {
                m_writer.key("loc");
                m_writer.beginObject();
                m_writer.kv("line", static_cast<uint64_t>(node->loc.line()));
                m_writer.kv("col", static_cast<uint64_t>(node->loc.column()));
                m_writer.endObject();
            }
            if (node->hasSyntaxError)
            {
                m_writer.kv("hasSyntaxError", true);
            }
        }

        /// Write an InternedString as a JSON string, or null if invalid.
        void writeInterned(InternedString s)
        {
            if (s.isValid())
            {
                m_writer.value(lookup(s));
            }
            else
            {
                m_writer.value(nullptr);
            }
        }

        /// Write a node, or JSON null for a null pointer.
        void writeNode(const BaseAST *node)
        {
            if (!node)
            {
                m_writer.value(nullptr);
                return;
            }

            switch (node->kind)
            {
            case ASTKind::Module:
                writeModule(node->as<ModuleAST>());
                break;
            case ASTKind::Attribute:
                writeAttribute(node->as<AttributeAST>());
                break;
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
            case ASTKind::CompositeInput:
                writeCompositeInput(node->as<CompositeInputAST>());
                break;
            case ASTKind::CompositeOutput:
                writeCompositeOutput(node->as<CompositeOutputAST>());
                break;
            case ASTKind::Decl:
            case ASTKind::Unknown:
                // The family base and the error placeholder have no
                // specific fields; the header is all we can write.
                m_writer.beginObject();
                writeNodeHeader(node);
                m_writer.endObject();
                break;
            }
        }

        /// Write an array of nodes. The element type is any pointer to
        /// a BaseAST-derived type.
        template <typename T>
        void writeNodeArray(ArenaSpan<T *> span)
        {
            m_writer.beginArray();
            for (T *item : span)
            {
                writeNode(item);
            }
            m_writer.endArray();
        }

        // ─── Module ────────────────────────────────────────────────────────

        void writeModule(const ModuleAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);

            m_writer.kv("filePath", lookup(node->filePath));

            m_writer.key("declarations");
            writeNodeArray(node->decls);

            m_writer.endObject();
        }

        // ─── Attribute ─────────────────────────────────────────────────────

        void writeAttribute(const AttributeAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("name", lookup(node->name));
            m_writer.endObject();
        }

        // ─── Type ──────────────────────────────────────────────────────────

        void writeTypeId(const TypeIdAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);

            m_writer.key("qualifier");
            writeInterned(node->qualifier);
            m_writer.kv("name", lookup(node->name));

            m_writer.endObject();
        }

        // ─── Values ────────────────────────────────────────────────────────

        void writeNodeExpr(const NodeExprAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);

            m_writer.key("type");
            writeNode(node->type);

            m_writer.key("args");
            writeNodeArray(node->args);

            m_writer.endObject();
        }

        void writeLiteralValue(const LiteralValueAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("literalKind", literalKindName(node->kind));
            m_writer.kv("text", lookup(node->text));
            m_writer.endObject();
        }

        void writeIdentifierValue(const IdentifierValueAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("name", lookup(node->name));
            m_writer.endObject();
        }

        void writeFieldAccessValue(const FieldAccessValueAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("object", lookup(node->object));
            m_writer.kv("field", lookup(node->field));
            m_writer.endObject();
        }

        void writeInlineNodeValue(const InlineNodeValueAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.key("node");
            writeNode(node->node);
            m_writer.endObject();
        }

        // ─── Declarations ──────────────────────────────────────────────────

        void writeAttributes(ArenaSpan<AttributeAST *> attrs)
        {
            m_writer.key("attributes");
            writeNodeArray(attrs);
        }

        void writeImportDecl(const ImportDeclAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("name", lookup(node->name)); // local alias
            m_writer.kv("path", lookup(node->path));
            writeAttributes(node->attributes);
            m_writer.endObject();
        }

        void writeEnumDecl(const EnumDeclAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("name", lookup(node->name));

            m_writer.key("members");
            m_writer.beginArray();
            for (InternedString m : node->members)
            {
                m_writer.value(lookup(m));
            }
            m_writer.endArray();

            writeAttributes(node->attributes);
            m_writer.endObject();
        }

        void writeResourceDecl(const ResourceDeclAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("name", lookup(node->name));

            m_writer.key("fields");
            writeNodeArray(node->fields);

            writeAttributes(node->attributes);
            m_writer.endObject();
        }

        void writeResourceField(const ResourceFieldAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("name", lookup(node->name));

            m_writer.key("type");
            writeNode(node->type);

            m_writer.key("default");
            writeNode(node->defaultValue);

            m_writer.endObject();
        }

        void writeNodeDecl(const NodeDeclAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("name", lookup(node->name));

            m_writer.key("expr");
            writeNode(node->expr);

            m_writer.key("triggers");
            m_writer.beginArray();
            for (InternedString t : node->triggers)
            {
                m_writer.value(lookup(t));
            }
            m_writer.endArray();

            writeAttributes(node->attributes);
            m_writer.endObject();
        }

        void writeCompositeDecl(const CompositeDeclAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("name", lookup(node->name));

            m_writer.key("inputs");
            writeNodeArray(node->inputs);

            m_writer.key("outputs");
            writeNodeArray(node->outputs);

            m_writer.key("body");
            writeNodeArray(node->body);

            writeAttributes(node->attributes);
            m_writer.endObject();
        }

        void writeCompositeInput(const CompositeInputAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("name", lookup(node->name));

            m_writer.key("type");
            writeNode(node->type);

            m_writer.endObject();
        }

        void writeCompositeOutput(const CompositeOutputAST *node)
        {
            m_writer.beginObject();
            writeNodeHeader(node);
            m_writer.kv("name", lookup(node->name));

            m_writer.key("type");
            writeNode(node->type);

            m_writer.key("value");
            writeNode(node->value);

            m_writer.endObject();
        }

        // ─── State ─────────────────────────────────────────────────────────

        JSONWriter m_writer;
        const StringPool &m_pool;
    };

    // ─── Public API ──────────────────────────────────────────────────────────

    std::string dumpModule(const ModuleAST *module, const StringPool &pool)
    {
        AstDumper dumper(pool);
        return dumper.dump(module);
    }

} // namespace lucid::parser::dump