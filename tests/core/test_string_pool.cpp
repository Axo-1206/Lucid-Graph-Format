/// @file tests/core/test_string_pool.cpp
///
/// @brief Tests for StringPool and InternedString.

#include "core/memory/InternedString.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>

// ─────────────────────────────────────────────────────────────────────────────
// InternedString
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("InternedString: default is invalid", "[core][intern]") {
    InternedString s;
    REQUIRE(s.id == 0);
    REQUIRE_FALSE(s.isValid());
    REQUIRE(s.isEmpty());
}

TEST_CASE("InternedString: equality is id equality", "[core][intern]") {
    InternedString a{1};
    InternedString b{1};
    InternedString c{2};

    REQUIRE(a == b);
    REQUIRE(a != c);
    REQUIRE(a.isValid());
    REQUIRE_FALSE(a.isEmpty());
}

TEST_CASE("InternedString: ordering is id ordering", "[core][intern]") {
    REQUIRE(InternedString{1} < InternedString{2});
    REQUIRE_FALSE(InternedString{2} < InternedString{1});
    REQUIRE_FALSE(InternedString{1} < InternedString{1});
}

TEST_CASE("InternedString: hash equals hash of the underlying id",
          "[core][intern]") {
    InternedString a{42};
    InternedString b{42};

    std::hash<InternedString> h;
    REQUIRE(h(a) == h(b));
    REQUIRE(h(a) == std::hash<uint32_t>{}(42));
}

TEST_CASE("InternedString: usable as unordered_map key",
          "[core][intern]") {
    std::unordered_map<InternedString, int> map;
    map[InternedString{1}] = 100;
    map[InternedString{2}] = 200;

    REQUIRE(map[InternedString{1}] == 100);
    REQUIRE(map[InternedString{2}] == 200);
    REQUIRE(map.find(InternedString{3}) == map.end());
}

TEST_CASE("InternedString: usable as unordered_set element",
          "[core][intern]") {
    std::unordered_set<InternedString> set;
    set.insert(InternedString{1});
    set.insert(InternedString{1});
    set.insert(InternedString{2});

    REQUIRE(set.size() == 2);
    REQUIRE(set.count(InternedString{1}) == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// StringPool: interning
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("StringPool: empty input interns to ID 0", "[core][pool]") {
    StringPool pool;
    REQUIRE(pool.intern("").id == 0);
    REQUIRE(pool.intern(std::string_view{}).id == 0);
}

TEST_CASE("StringPool: first non-empty string is ID 1", "[core][pool]") {
    StringPool pool;
    InternedString s = pool.intern("hello");
    REQUIRE(s.id == 1);
    REQUIRE(s.isValid());
}

TEST_CASE("StringPool: identical strings share one handle", "[core][pool]") {
    StringPool pool;
    InternedString a = pool.intern("hello");
    InternedString b = pool.intern("hello");
    InternedString c = pool.intern(std::string("hello"));  // temporary

    REQUIRE(a == b);
    REQUIRE(a == c);
    REQUIRE(a.id == b.id);
    REQUIRE(a.id == c.id);
}

TEST_CASE("StringPool: distinct strings get distinct handles",
          "[core][pool]") {
    StringPool pool;
    InternedString a = pool.intern("hello");
    InternedString b = pool.intern("world");
    InternedString c = pool.intern("hellO");   // case-sensitive

    REQUIRE(a != b);
    REQUIRE(a != c);
    REQUIRE(b != c);
}

TEST_CASE("StringPool: interning after many strings still works",
          "[core][pool]") {
    StringPool pool;
    // Enough to force at least one block boundary.
    for (int i = 0; i < 10000; ++i) {
        pool.intern("string_" + std::to_string(i));
    }
    // Every string we just interned must still be reachable.
    for (int i = 0; i < 10000; ++i) {
        InternedString s = pool.intern("string_" + std::to_string(i));
        REQUIRE(pool.lookupView(s) == "string_" + std::to_string(i));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// StringPool: lookup
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("StringPool: lookupView returns the interned text",
          "[core][pool]") {
    StringPool pool;
    InternedString s = pool.intern("hello");
    REQUIRE(pool.lookupView(s) == "hello");
    REQUIRE(pool.lookup(s) == "hello");
}

TEST_CASE("StringPool: lookupView of ID 0 returns empty view",
          "[core][pool]") {
    StringPool pool;
    REQUIRE(pool.lookupView(InternedString{}).empty());
}

TEST_CASE("StringPool: lookupView of an out-of-range ID returns empty",
          "[core][pool]") {
    StringPool pool;
    pool.intern("hello");
    // ID 9999 was never issued.
    REQUIRE(pool.lookupView(InternedString{9999}).empty());
}

TEST_CASE("StringPool: size counts the empty string", "[core][pool]") {
    StringPool pool;
    REQUIRE(pool.size() == 1);      // ID 0 for empty
    pool.intern("a");
    REQUIRE(pool.size() == 2);
    pool.intern("a");               // duplicate, no growth
    REQUIRE(pool.size() == 2);
    pool.intern("b");
    REQUIRE(pool.size() == 3);
}

// ─────────────────────────────────────────────────────────────────────────────
// StringPool: contains
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("StringPool: contains is a bounds check", "[core][pool]") {
    StringPool pool;
    InternedString a = pool.intern("hello");
    REQUIRE(pool.contains(a));
    REQUIRE(pool.contains(InternedString{}));    // ID 0 is in range
    REQUIRE_FALSE(pool.contains(InternedString{9999}));
}

// ─────────────────────────────────────────────────────────────────────────────
// StringPool: block invariant
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("StringPool: views remain valid after interning more strings",
          "[core][pool]") {
    StringPool pool;

    // Intern a string, capture its view.
    InternedString a = pool.intern("first");
    std::string_view viewA = pool.lookupView(a);

    // Intern enough other strings to force new blocks.
    for (int i = 0; i < 10000; ++i) {
        pool.intern("filler_" + std::to_string(i));
    }

    // The original view is still valid and still points at "first".
    REQUIRE(viewA == "first");
    REQUIRE(pool.lookupView(a) == "first");
}

TEST_CASE("StringPool: interning from a temporary is safe", "[core][pool]") {
    StringPool pool;

    // This creates a temporary std::string, interns it, and destroys it.
    // The pool must have copied the bytes; the handle must remain valid.
    InternedString s = pool.intern(std::string("temporary"));
    REQUIRE(pool.lookupView(s) == "temporary");

    // A stack buffer.
    char buf[] = "stack";
    InternedString t = pool.intern(buf);
    REQUIRE(pool.lookupView(t) == "stack");
}