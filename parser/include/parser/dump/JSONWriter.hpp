/// @file parser/dump/JSONWriter.hpp
///
/// @brief A small streaming JSON writer.
///
/// ─── What this is ─────────────────────────────────────────────────────────
/// A JSON emitter. It writes JSON tokens (objects, arrays, strings,
/// numbers, booleans, null) to an in-memory buffer. It does not parse
/// JSON, does not hold a tree, and does not know anything about the AST.
/// The AST dumper drives it.
///
/// ─── What this is not ─────────────────────────────────────────────────────
/// It is not a general-purpose JSON library. It has no reader, no DOM,
/// no schema, no error recovery, and no pretty-printing. It assumes it is
/// being called correctly: every `beginObject` is matched by an
/// `endObject`, every `key` is followed by a value, and so on. Misuse
/// produces malformed JSON, not an exception.
///
/// ─── Compact output, always ───────────────────────────────────────────────
/// The writer emits a single line of compact JSON. There is no
/// indentation, no newlines, no optional pretty mode. Human readers open
/// the output in an editor, which pretty-prints it for them. The fixture
/// tests compare the compact text directly, so the output must be
/// deterministic and stable; pretty-printing is the caller's job.
///
/// ─── Comma handling ───────────────────────────────────────────────────────
/// The writer tracks a stack of open containers. Each entry records the
/// container's kind (object or array) and whether it is still empty. A
/// comma is inserted before a member when the enclosing container is
/// non-empty. The caller never tracks this.
///
/// ─── The `const char*` overloads are load-bearing ─────────────────────────
/// `w.value("hello")` must select the string overload, not `value(bool)`.
/// A `const char*` argument is a standard-conversion match for `bool` and
/// a user-defined conversion match for `std::string_view`. Without an
/// explicit `const char*` overload, the `bool` overload wins and the
/// string is written as `true`. The `const char*` overloads below are not
/// redundant; removing them silently breaks every string literal.

#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace lucid::parser::dump
{

    class JSONWriter
    {
    public:
        JSONWriter() = default;

        // ─── Delimiters ─────────────────────────────────────────────────────

        void beginObject()
        {
            prefixValue();
            m_buffer += '{';
            m_stack.push_back(Container::Object);
            m_empty.push_back(true);
        }

        void endObject()
        {
            m_stack.pop_back();
            m_empty.pop_back();
            m_buffer += '}';
        }

        void beginArray()
        {
            prefixValue();
            m_buffer += '[';
            m_stack.push_back(Container::Array);
            m_empty.push_back(true);
        }

        void endArray()
        {
            m_stack.pop_back();
            m_empty.pop_back();
            m_buffer += ']';
        }

        // ─── Keys ───────────────────────────────────────────────────────────

        void key(std::string_view k)
        {
            prefixKey();
            writeStringLiteral(k);
            m_buffer += ':';
        }

        // ─── Values ─────────────────────────────────────────────────────────

        void value(std::nullptr_t)
        {
            prefixValue();
            m_buffer += "null";
        }

        void value(bool v)
        {
            prefixValue();
            m_buffer += v ? "true" : "false";
        }

        void value(int64_t v)
        {
            prefixValue();
            char buf[24];
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
            if (ec == std::errc{})
            {
                m_buffer.append(buf, ptr);
            }
            else
            {
                m_buffer += std::to_string(v);
            }
        }

        void value(uint64_t v)
        {
            prefixValue();
            char buf[24];
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
            if (ec == std::errc{})
            {
                m_buffer.append(buf, ptr);
            }
            else
            {
                m_buffer += std::to_string(v);
            }
        }

        void value(double v)
        {
            prefixValue();
            if (std::isnan(v) || std::isinf(v))
            {
                // JSON has no NaN or Infinity. Emit null.
                m_buffer += "null";
                return;
            }
            char buf[32];
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
            if (ec == std::errc{})
            {
                m_buffer.append(buf, ptr);
            }
            else
            {
                std::snprintf(buf, sizeof(buf), "%.17g", v);
                m_buffer += buf;
            }
        }

        void value(std::string_view v)
        {
            prefixValue();
            writeStringLiteral(v);
        }

        // The overload that makes string literals work. See the file's
        // doc comment on overload ordering.
        void value(const char *v)
        {
            value(std::string_view{v});
        }

        // ─── key + value ────────────────────────────────────────────────────

        void kv(std::string_view k, std::nullptr_t)
        {
            key(k);
            value(nullptr);
        }
        void kv(std::string_view k, bool v)
        {
            key(k);
            value(v);
        }
        void kv(std::string_view k, int64_t v)
        {
            key(k);
            value(v);
        }
        void kv(std::string_view k, uint64_t v)
        {
            key(k);
            value(v);
        }
        void kv(std::string_view k, double v)
        {
            key(k);
            value(v);
        }
        void kv(std::string_view k, std::string_view v)
        {
            key(k);
            value(v);
        }
        void kv(std::string_view k, const char *v)
        {
            key(k);
            value(v);
        }

        // ─── Empty containers ───────────────────────────────────────────────

        void emptyObject()
        {
            prefixValue();
            m_buffer += "{}";
        }

        void emptyArray()
        {
            prefixValue();
            m_buffer += "[]";
        }

        // ─── The output ─────────────────────────────────────────────────────

        std::string str() const { return m_buffer; }

    private:
        enum class Container
        {
            Object,
            Array
        };

        // Called before writing a value. In an array, this writes the
        // comma. In an object, the key already wrote the comma (or
        // there was no preceding member). At the top level, nothing.
        void prefixValue()
        {
            if (m_stack.empty())
            {
                return;
            }
            if (m_stack.back() == Container::Array)
            {
                writeCommaIfNeeded();
            }
            m_empty.back() = false;
        }

        // Called before writing a key. This always begins an object member.
        void prefixKey()
        {
            writeCommaIfNeeded();
            m_empty.back() = false;
        }

        void writeCommaIfNeeded()
        {
            if (m_stack.empty())
            {
                return;
            }
            if (!m_empty.back())
            {
                m_buffer += ',';
            }
        }

        // ─── String escaping ────────────────────────────────────────────────

        void writeStringLiteral(std::string_view s)
        {
            m_buffer += '"';
            for (char c : s)
            {
                switch (c)
                {
                case '"':
                    m_buffer += "\\\"";
                    break;
                case '\\':
                    m_buffer += "\\\\";
                    break;
                case '\b':
                    m_buffer += "\\b";
                    break;
                case '\f':
                    m_buffer += "\\f";
                    break;
                case '\n':
                    m_buffer += "\\n";
                    break;
                case '\r':
                    m_buffer += "\\r";
                    break;
                case '\t':
                    m_buffer += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x",
                                      static_cast<unsigned>(
                                          static_cast<unsigned char>(c)));
                        m_buffer += buf;
                    }
                    else
                    {
                        m_buffer += c;
                    }
                    break;
                }
            }
            m_buffer += '"';
        }

        // ─── State ──────────────────────────────────────────────────────────

        std::string m_buffer;
        std::vector<Container> m_stack;
        std::vector<bool> m_empty;
    };

} // namespace lucid::parser::dump