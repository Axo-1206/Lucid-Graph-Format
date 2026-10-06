/// @file tests/parser/test_json_writer.cpp
///
/// @brief Tests for JSONWriter.
///
/// ─── Test shape ───────────────────────────────────────────────────────────
/// Each test constructs a writer, drives it with a small sequence of
/// calls, and compares the exact output string. No JSON parser is
/// involved; the tests assert on the exact bytes the writer produces.
///
/// The writer produces compact output only. There is no pretty mode and
/// no pretty-specific test.

#include "parser/dump/JSONWriter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>

using lucid::parser::dump::JSONWriter;

// ─────────────────────────────────────────────────────────────────────────────
// Scalars
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("JSONWriter writes null", "[json-writer]")
{
    JSONWriter w;
    w.value(nullptr);
    CHECK(w.str() == "null");
}

TEST_CASE("JSONWriter writes true", "[json-writer]")
{
    JSONWriter w;
    w.value(true);
    CHECK(w.str() == "true");
}

TEST_CASE("JSONWriter writes false", "[json-writer]")
{
    JSONWriter w;
    w.value(false);
    CHECK(w.str() == "false");
}

TEST_CASE("JSONWriter writes an integer", "[json-writer]")
{
    JSONWriter w;
    w.value(int64_t{42});
    CHECK(w.str() == "42");
}

TEST_CASE("JSONWriter writes a negative integer", "[json-writer]")
{
    JSONWriter w;
    w.value(int64_t{-7});
    CHECK(w.str() == "-7");
}

TEST_CASE("JSONWriter writes an unsigned integer", "[json-writer]")
{
    JSONWriter w;
    w.value(uint64_t{12345678901234567890ull});
    CHECK(w.str() == "12345678901234567890");
}

TEST_CASE("JSONWriter writes a double", "[json-writer]")
{
    JSONWriter w;
    w.value(3.14);
    CHECK(w.str() == "3.14");
}

TEST_CASE("JSONWriter writes NaN as null", "[json-writer]")
{
    JSONWriter w;
    w.value(std::nan(""));
    CHECK(w.str() == "null");
}

TEST_CASE("JSONWriter writes Infinity as null", "[json-writer]")
{
    JSONWriter w;
    w.value(std::numeric_limits<double>::infinity());
    CHECK(w.str() == "null");
}

// ─────────────────────────────────────────────────────────────────────────────
// Strings
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("JSONWriter writes a simple string", "[json-writer]")
{
    JSONWriter w;
    w.value("hello");
    CHECK(w.str() == "\"hello\"");
}

TEST_CASE("JSONWriter writes a std::string_view string", "[json-writer]")
{
    JSONWriter w;
    w.value(std::string_view{"hello"});
    CHECK(w.str() == "\"hello\"");
}

TEST_CASE("JSONWriter writes a std::string string", "[json-writer]")
{
    JSONWriter w;
    w.value(std::string{"hello"});
    CHECK(w.str() == "\"hello\"");
}

TEST_CASE("JSONWriter escapes a double quote", "[json-writer]")
{
    JSONWriter w;
    w.value("a\"b");
    CHECK(w.str() == "\"a\\\"b\"");
}

TEST_CASE("JSONWriter escapes a backslash", "[json-writer]")
{
    JSONWriter w;
    w.value("a\\b");
    CHECK(w.str() == "\"a\\\\b\"");
}

TEST_CASE("JSONWriter escapes a newline", "[json-writer]")
{
    JSONWriter w;
    w.value("a\nb");
    CHECK(w.str() == "\"a\\nb\"");
}

TEST_CASE("JSONWriter escapes a tab", "[json-writer]")
{
    JSONWriter w;
    w.value("a\tb");
    CHECK(w.str() == "\"a\\tb\"");
}

TEST_CASE("JSONWriter escapes a carriage return", "[json-writer]")
{
    JSONWriter w;
    w.value("a\rb");
    CHECK(w.str() == "\"a\\rb\"");
}

TEST_CASE("JSONWriter escapes a backspace and form feed", "[json-writer]")
{
    JSONWriter w;
    w.value("a\bb\fc");
    CHECK(w.str() == "\"a\\bb\\fc\"");
}

TEST_CASE("JSONWriter escapes a control character", "[json-writer]")
{
    JSONWriter w;
    w.value(std::string_view{"a\x01"
                             "b",
                             3});
    CHECK(w.str() == "\"a\\u0001b\"");
}

TEST_CASE("JSONWriter does not escape non-ASCII bytes", "[json-writer]")
{
    JSONWriter w;
    w.value(std::string_view{"\xc3\xa9", 2});
    CHECK(w.str() == "\"\xc3\xa9\"");
}

// ─────────────────────────────────────────────────────────────────────────────
// Objects
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("JSONWriter writes an empty object", "[json-writer]")
{
    JSONWriter w;
    w.beginObject();
    w.endObject();
    CHECK(w.str() == "{}");
}

TEST_CASE("JSONWriter writes a one-member object", "[json-writer]")
{
    JSONWriter w;
    w.beginObject();
    w.kv("a", int64_t{1});
    w.endObject();
    CHECK(w.str() == "{\"a\":1}");
}

TEST_CASE("JSONWriter writes a two-member object", "[json-writer]")
{
    JSONWriter w;
    w.beginObject();
    w.kv("a", int64_t{1});
    w.kv("b", int64_t{2});
    w.endObject();
    CHECK(w.str() == "{\"a\":1,\"b\":2}");
}

TEST_CASE("JSONWriter writes a string member", "[json-writer]")
{
    JSONWriter w;
    w.beginObject();
    w.kv("name", "speed");
    w.endObject();
    CHECK(w.str() == "{\"name\":\"speed\"}");
}

TEST_CASE("JSONWriter handles a nested object", "[json-writer]")
{
    JSONWriter w;
    w.beginObject();
    w.key("outer");
    w.beginObject();
    w.kv("inner", int64_t{1});
    w.endObject();
    w.endObject();
    CHECK(w.str() == "{\"outer\":{\"inner\":1}}");
}

TEST_CASE("JSONWriter handles sibling nested objects", "[json-writer]")
{
    JSONWriter w;
    w.beginObject();
    w.key("a");
    w.beginObject();
    w.kv("x", int64_t{1});
    w.endObject();
    w.key("b");
    w.beginObject();
    w.kv("y", int64_t{2});
    w.endObject();
    w.endObject();
    CHECK(w.str() == "{\"a\":{\"x\":1},\"b\":{\"y\":2}}");
}

// ─────────────────────────────────────────────────────────────────────────────
// Arrays
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("JSONWriter writes an empty array", "[json-writer]")
{
    JSONWriter w;
    w.beginArray();
    w.endArray();
    CHECK(w.str() == "[]");
}

TEST_CASE("JSONWriter writes a one-element array", "[json-writer]")
{
    JSONWriter w;
    w.beginArray();
    w.value(int64_t{1});
    w.endArray();
    CHECK(w.str() == "[1]");
}

TEST_CASE("JSONWriter writes a multi-element array", "[json-writer]")
{
    JSONWriter w;
    w.beginArray();
    w.value(int64_t{1});
    w.value(int64_t{2});
    w.value(int64_t{3});
    w.endArray();
    CHECK(w.str() == "[1,2,3]");
}

TEST_CASE("JSONWriter writes an array of strings", "[json-writer]")
{
    JSONWriter w;
    w.beginArray();
    w.value("a");
    w.value("b");
    w.endArray();
    CHECK(w.str() == "[\"a\",\"b\"]");
}

TEST_CASE("JSONWriter handles an array of objects", "[json-writer]")
{
    JSONWriter w;
    w.beginArray();
    {
        w.beginObject();
        w.kv("a", int64_t{1});
        w.endObject();
    }
    {
        w.beginObject();
        w.kv("b", int64_t{2});
        w.endObject();
    }
    w.endArray();
    CHECK(w.str() == "[{\"a\":1},{\"b\":2}]");
}

TEST_CASE("JSONWriter handles a nested array", "[json-writer]")
{
    JSONWriter w;
    w.beginArray();
    w.beginArray();
    w.value(int64_t{1});
    w.endArray();
    w.endArray();
    CHECK(w.str() == "[[1]]");
}

// ─────────────────────────────────────────────────────────────────────────────
// The empty-container shortcuts
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("JSONWriter emptyObject writes {}", "[json-writer]")
{
    JSONWriter w;
    w.beginObject();
    w.kv("a", int64_t{1});
    w.key("b");
    w.emptyObject();
    w.endObject();
    CHECK(w.str() == "{\"a\":1,\"b\":{}}");
}

TEST_CASE("JSONWriter emptyArray writes []", "[json-writer]")
{
    JSONWriter w;
    w.beginObject();
    w.kv("a", int64_t{1});
    w.key("b");
    w.emptyArray();
    w.endObject();
    CHECK(w.str() == "{\"a\":1,\"b\":[]}");
}

// ─────────────────────────────────────────────────────────────────────────────
// The string-literal overload trap
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("JSONWriter treats a string literal as a string, not a bool",
          "[json-writer]")
{
    // This test exists because the `const char*` overload is
    // load-bearing. Without it, `w.value("hello")` selects
    // `value(bool)` and the output is `true`, not `"hello"`.
    JSONWriter w;
    w.value("hello");
    CHECK(w.str() == "\"hello\"");
    CHECK(w.str() != "true");
}

TEST_CASE("JSONWriter's kv with a string literal writes a string",
          "[json-writer]")
{
    JSONWriter w;
    w.beginObject();
    w.kv("name", "speed");
    w.endObject();
    CHECK(w.str() == "{\"name\":\"speed\"}");
    CHECK(w.str() != "{\"name\":true}");
}

// ─────────────────────────────────────────────────────────────────────────────
// A realistic mixed structure
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("JSONWriter writes a realistic mixed structure", "[json-writer]")
{
    // The AST dumper's shape, in miniature:
    //   { "kind": "NodeDecl", "name": "speed",
    //     "args": [ {"kind": "LiteralValue", "text": "200.0"} ] }
    JSONWriter w;
    w.beginObject();
    w.kv("kind", "NodeDecl");
    w.kv("name", "speed");
    w.key("args");
    w.beginArray();
    {
        w.beginObject();
        w.kv("kind", "LiteralValue");
        w.kv("text", "200.0");
        w.endObject();
    }
    w.endArray();
    w.endObject();

    CHECK(w.str() ==
          "{\"kind\":\"NodeDecl\",\"name\":\"speed\","
          "\"args\":[{\"kind\":\"LiteralValue\",\"text\":\"200.0\"}]}");
}

TEST_CASE("JSONWriter handles a deeply nested structure", "[json-writer]")
{
    // { "a": [ { "b": [ { "c": 1 } ] } ] }
    JSONWriter w;
    w.beginObject();
    w.key("a");
    w.beginArray();
    w.beginObject();
    w.key("b");
    w.beginArray();
    w.beginObject();
    w.kv("c", int64_t{1});
    w.endObject();
    w.endArray();
    w.endObject();
    w.endArray();
    w.endObject();
    CHECK(w.str() == "{\"a\":[{\"b\":[{\"c\":1}]}]}");
}