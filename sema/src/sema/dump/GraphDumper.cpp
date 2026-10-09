/// @file sema/src/sema/dump/GraphDumper.cpp
///
/// @brief Implementation of the Graph JSON dumper.

#include "sema/dump/GraphDumper.hpp"

#include "parser/dump/JSONWriter.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace lucid::sema::dump
{

    namespace
    {

        using lucid::parser::dump::JSONWriter;

        /// Emit a signed integer as a JSON string (no precision loss).
        void writeIntAsString(JSONWriter &w, int64_t v)
        {
            const std::string s = std::to_string(v);
            w.value(s);
        }

        void writeUIntAsString(JSONWriter &w, uint64_t v)
        {
            const std::string s = std::to_string(v);
            w.value(s);
        }

        void writeLiteral(JSONWriter &w, const Literal &lit)
        {
            w.beginObject();
            w.kv("kind", sema::literalKindName(lit.kind));
            switch (lit.kind)
            {
            case Literal::Kind::Nil:
                break;
            case Literal::Kind::Bool:
                w.kv("value", lit.b);
                break;
            case Literal::Kind::Char:
                w.kv("value", static_cast<uint64_t>(
                                  static_cast<unsigned char>(lit.c)));
                break;
            case Literal::Kind::String:
                w.kv("offset", static_cast<uint64_t>(lit.string.offset));
                w.kv("length", static_cast<uint64_t>(lit.string.length));
                break;
            case Literal::Kind::Int8:
            case Literal::Kind::Int16:
            case Literal::Kind::Int32:
            case Literal::Kind::Int64:
                w.key("value");
                writeIntAsString(w, lit.i);
                break;
            case Literal::Kind::UInt8:
            case Literal::Kind::UInt16:
            case Literal::Kind::UInt32:
            case Literal::Kind::UInt64:
                w.key("value");
                writeUIntAsString(w, lit.u);
                break;
            case Literal::Kind::Float32:
            case Literal::Kind::Float64:
                w.kv("value", lit.f);
                break;
            }
            w.endObject();
        }

        // ─── TypeId encoding ──────────────────────────────────────────────

        void writeTypeId(JSONWriter &w, const TypeId &t)
        {
            w.beginObject();
            w.kv("kind", typeKindName(t.kind));
            w.kv("name", t.name);
            w.endObject();
        }

        // ─── Arg encoding ─────────────────────────────────────────────────

        void writeArg(JSONWriter &w, const Arg &arg)
        {
            w.beginObject();
            switch (arg.kind)
            {
            case Arg::Kind::Literal:
                w.kv("kind", "Literal");
                w.key("literal");
                writeLiteral(w, arg.literal);
                break;
            case Arg::Kind::NodeRef:
                w.kv("kind", "NodeRef");
                w.kv("node", static_cast<uint64_t>(arg.node_ref));
                break;
            case Arg::Kind::ResourceRef:
                w.kv("kind", "ResourceRef");
                w.kv("resource", static_cast<uint64_t>(
                                     arg.resource_ref.resource_index));
                w.kv("field", static_cast<uint64_t>(
                                  arg.resource_ref.field_index));
                break;
            }
            w.endObject();
        }

        // ─── Node encoding ────────────────────────────────────────────────

        void writeNode(JSONWriter &w, const Graph &g,
                       NodeIndex idx, const NodeInstance &node)
        {
            w.beginObject();
            w.kv("index", static_cast<uint64_t>(idx));
            w.kv("typeId", static_cast<uint64_t>(node.type_id));
            w.kv("phase", static_cast<uint64_t>(node.phase));

            w.key("args");
            w.beginArray();
            auto args = g.argsOf(node);
            for (const Arg &a : args)
            {
                writeArg(w, a);
            }
            w.endArray();

            w.key("subscribers");
            w.beginArray();
            auto subs = g.subscribersOf(node);
            for (NodeIndex s : subs)
            {
                w.value(static_cast<uint64_t>(s));
            }
            w.endArray();

            w.endObject();
        }

        // ─── Resource encoding ────────────────────────────────────────────

        void writeResourceField(JSONWriter &w, const ResourceField &f)
        {
            w.beginObject();
            w.kv("name", f.name);
            w.key("type");
            writeTypeId(w, f.type);
            w.kv("hasDefault", f.hasDefault);
            if (f.hasDefault)
            {
                w.key("default");
                writeLiteral(w, f.defaultValue);
            }
            w.endObject();
        }

        void writeResource(JSONWriter &w, const Graph &g,
                           uint32_t idx, const Resource &r)
        {
            w.beginObject();
            w.kv("index", static_cast<uint64_t>(idx));
            w.kv("name", r.name);

            w.key("fields");
            w.beginArray();
            auto fields = g.fieldsOf(r);
            for (const ResourceField &f : fields)
            {
                writeResourceField(w, f);
            }
            w.endArray();

            w.endObject();
        }

    } // namespace

    // ─── Public entry point ───────────────────────────────────────────────────

    std::string dumpGraph(const Graph &graph)
    {
        JSONWriter w;

        w.beginObject();

        // ─── Registry fingerprint ──────────────────────────────────────────
        w.key("registryFingerprint");
        writeUIntAsString(w, graph.registry_fingerprint);

        // ─── String pool ───────────────────────────────────────────────────
        w.key("stringPool");
        w.beginObject();
        w.kv("size", static_cast<uint64_t>(graph.string_pool.size()));
        w.kv("bytes",
             std::string_view(graph.string_pool.data(),
                              graph.string_pool.size()));
        w.endObject();

        // ─── Nodes ─────────────────────────────────────────────────────────
        w.key("nodes");
        w.beginArray();
        for (size_t i = 0; i < graph.nodes.size(); ++i)
        {
            writeNode(w, graph, static_cast<NodeIndex>(i), graph.nodes[i]);
        }
        w.endArray();

        // ─── Resources ─────────────────────────────────────────────────────
        w.key("resources");
        w.beginArray();
        for (size_t i = 0; i < graph.resources.size(); ++i)
        {
            writeResource(w, graph, static_cast<uint32_t>(i),
                          graph.resources[i]);
        }
        w.endArray();

        // ─── Execution orders ──────────────────────────────────────────────
        w.key("phaseOrder");
        w.beginArray();
        for (NodeIndex n : graph.phase_order)
        {
            w.value(static_cast<uint64_t>(n));
        }
        w.endArray();

        w.key("valueOrder");
        w.beginArray();
        for (NodeIndex n : graph.value_order)
        {
            w.value(static_cast<uint64_t>(n));
        }
        w.endArray();

        w.endObject();

        return w.str();
    }

} // namespace lucid::sema::dump