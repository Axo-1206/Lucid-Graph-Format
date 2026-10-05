/// @file tests/core/test_ast_arena.cpp
///
/// @brief Tests for ASTArena and ArenaSpan.

#include "core/memory/ASTArena.hpp"
#include "core/memory/ArenaSpan.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// A test node type
// ─────────────────────────────────────────────────────────────────────────────
//
// ASTArena's alloc<T> placement-news T. Give the test a type with a real
// constructor so the forwarding is exercised.

namespace {

struct TestNode {
  int value;
  std::string name;

  TestNode(int v, std::string n) : value(v), name(std::move(n)) {}
};

struct Aligned8 {
  alignas(8) uint64_t a;
  uint64_t b;
};

struct Aligned64 {
  alignas(64) uint64_t a;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// ArenaSpan
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ArenaSpan: default is empty", "[core][span]") {
  ArenaSpan<int> s;
  REQUIRE(s.empty());
  REQUIRE(s.size() == 0);
  REQUIRE(s.data() == nullptr);
  REQUIRE(s.begin() == s.end());
}

TEST_CASE("ArenaSpan: constructed from pointer and size", "[core][span]") {
  int arr[3] = {10, 20, 30};
  ArenaSpan<int> s{arr, 3};

  REQUIRE(s.size() == 3);
  REQUIRE_FALSE(s.empty());
  REQUIRE(s[0] == 10);
  REQUIRE(s[1] == 20);
  REQUIRE(s[2] == 30);
  REQUIRE(s.front() == 10);
  REQUIRE(s.back() == 30);
}

TEST_CASE("ArenaSpan: iteration visits every element", "[core][span]") {
  int arr[4] = {1, 2, 3, 4};
  ArenaSpan<int> s{arr, 4};

  int sum = 0;
  for (int v : s)
    sum += v;
  REQUIRE(sum == 10);
}

TEST_CASE("ArenaSpan: contains does a linear search", "[core][span]") {
  int arr[3] = {1, 2, 3};
  ArenaSpan<int> s{arr, 3};

  REQUIRE(s.contains(1));
  REQUIRE(s.contains(2));
  REQUIRE(s.contains(3));
  REQUIRE_FALSE(s.contains(4));
}

TEST_CASE("ArenaSpan: subspan clamps to the end", "[core][span]") {
  int arr[5] = {0, 1, 2, 3, 4};
  ArenaSpan<int> s{arr, 5};

  auto a = s.subspan(1, 2);
  REQUIRE(a.size() == 2);
  REQUIRE(a[0] == 1);
  REQUIRE(a[1] == 2);

  // count past the end clamps.
  auto b = s.subspan(3, 100);
  REQUIRE(b.size() == 2);
  REQUIRE(b[0] == 3);
  REQUIRE(b[1] == 4);

  // offset past the end is empty.
  auto c = s.subspan(10, 5);
  REQUIRE(c.empty());
}

TEST_CASE("ArenaSpan: slice returns the tail", "[core][span]") {
  int arr[5] = {0, 1, 2, 3, 4};
  ArenaSpan<int> s{arr, 5};

  auto a = s.slice(2);
  REQUIRE(a.size() == 3);
  REQUIRE(a[0] == 2);
  REQUIRE(a[2] == 4);

  auto b = s.slice(10);
  REQUIRE(b.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// ASTArena: allocation
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ASTArena: alloc constructs the object", "[core][arena]") {
  ASTArena arena;
  TestNode *n = arena.alloc<TestNode>(42, "foo");

  REQUIRE(n != nullptr);
  REQUIRE(n->value == 42);
  REQUIRE(n->name == "foo");
}

TEST_CASE("ASTArena: make is an alias for alloc", "[core][arena]") {
  ASTArena arena;
  TestNode *a = arena.alloc<TestNode>(1, "a");
  TestNode *b = arena.make<TestNode>(2, "b");

  REQUIRE(a->value == 1);
  REQUIRE(b->value == 2);
}

TEST_CASE("ASTArena: family aliases forward to alloc", "[core][arena]") {
  ASTArena arena;
  auto *d = arena.makeDecl<TestNode>(1, "d");
  auto *v = arena.makeValue<TestNode>(2, "v");
  auto *t = arena.makeType<TestNode>(3, "t");

  REQUIRE(d->value == 1);
  REQUIRE(v->value == 2);
  REQUIRE(t->value == 3);
}

TEST_CASE("ASTArena: pointers remain stable across allocations",
          "[core][arena]") {
  ASTArena arena;
  TestNode *first = arena.alloc<TestNode>(1, "first");

  // Allocate enough to force multiple blocks.
  for (int i = 0; i < 10000; ++i) {
    arena.alloc<TestNode>(i, "filler");
  }

  // The first pointer is unchanged.
  REQUIRE(first->value == 1);
  REQUIRE(first->name == "first");
}

TEST_CASE("ASTArena: respects alignment", "[core][arena]") {
  ASTArena arena;
  // Allocate some small things first to offset the cursor.
  arena.alloc<char>(0);
  arena.alloc<char>(0);

  auto *a = arena.alloc<Aligned8>();
  auto *b = arena.alloc<Aligned64>();

  REQUIRE(reinterpret_cast<uintptr_t>(a) % 8 == 0);
  REQUIRE(reinterpret_cast<uintptr_t>(b) % 64 == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// ASTArena: allocArray
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ASTArena: allocArray builds a span from an init list",
          "[core][arena]") {
  ASTArena arena;
  auto s = arena.allocArray<int>({1, 2, 3, 4});

  REQUIRE(s.size() == 4);
  REQUIRE(s[0] == 1);
  REQUIRE(s[3] == 4);
}

TEST_CASE("ASTArena: allocArray of an empty list is empty", "[core][arena]") {
  ASTArena arena;
  auto s = arena.allocArray<int>({});
  REQUIRE(s.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// ASTArena: SpanBuilder
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ASTArena: SpanBuilder builds a span", "[core][arena][builder]") {
  ASTArena arena;
  auto b = arena.makeBuilder<int>();
  b.push_back(1);
  b.push_back(2);
  b.push_back(3);
  auto s = b.build();

  REQUIRE(s.size() == 3);
  REQUIRE(s[0] == 1);
  REQUIRE(s[2] == 3);
}

TEST_CASE("ASTArena: SpanBuilder of nothing is empty",
          "[core][arena][builder]") {
  ASTArena arena;
  auto b = arena.makeBuilder<int>();
  auto s = b.build();
  REQUIRE(s.empty());
}

TEST_CASE("ASTArena: SpanBuilder emplace_back returns a reference",
          "[core][arena][builder]") {
  ASTArena arena;
  auto b = arena.makeBuilder<TestNode>();
  TestNode &n = b.emplace_back(7, "seven");
  REQUIRE(n.value == 7);
  REQUIRE(n.name == "seven");

  auto s = b.build();
  REQUIRE(s.size() == 1);
  REQUIRE(s[0].value == 7);
}

TEST_CASE("ASTArena: ReservedSpanBuilder pre-reserves",
          "[core][arena][builder]") {
  ASTArena arena;
  auto b = arena.makeBuilder<int>(100);
  REQUIRE(b.capacity() >= 100);

  for (int i = 0; i < 10; ++i)
    b.push_back(i);
  auto s = b.build();
  REQUIRE(s.size() == 10);
  REQUIRE(s[0] == 0);
  REQUIRE(s[9] == 9);
}

// ─────────────────────────────────────────────────────────────────────────────
// ASTArena: convenience span constructors
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ASTArena: makeSpan of one element", "[core][arena][makeSpan]") {
  ASTArena arena;
  auto s = arena.makeSpan<int>(42);
  REQUIRE(s.size() == 1);
  REQUIRE(s[0] == 42);
}

TEST_CASE("ASTArena: makeSpan from initializer list",
          "[core][arena][makeSpan]") {
  ASTArena arena;
  auto s = arena.makeSpan<int>({1, 2, 3});
  REQUIRE(s.size() == 3);
  REQUIRE(s[2] == 3);
}

TEST_CASE("ASTArena: makeSpan from vector", "[core][arena][makeSpan]") {
  ASTArena arena;
  std::vector<int> v = {4, 5, 6};
  auto s = arena.makeSpan<int>(v);
  REQUIRE(s.size() == 3);
  REQUIRE(s[1] == 5);
}

TEST_CASE("ASTArena: makeSpan from another span", "[core][arena][makeSpan]") {
  ASTArena arena;
  int arr[3] = {1, 2, 3};
  ArenaSpan<int> src{arr, 3};
  auto s = arena.makeSpan<int>(src);
  REQUIRE(s.size() == 3);
  REQUIRE(s[1] == 2);
}

TEST_CASE("ASTArena: makeSpan with a transform", "[core][arena][makeSpan]") {
  ASTArena arena;
  std::vector<int> v = {1, 2, 3};
  auto s = arena.makeSpan<int>(v, [](int x) { return x * 10; });
  REQUIRE(s.size() == 3);
  REQUIRE(s[0] == 10);
  REQUIRE(s[2] == 30);
}

// ─────────────────────────────────────────────────────────────────────────────
// ASTArena: concatenation
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ASTArena: concatSpans joins two spans", "[core][arena][concat]") {
  ASTArena arena;
  int a[2] = {1, 2};
  int b[3] = {3, 4, 5};
  ArenaSpan<int> sa{a, 2};
  ArenaSpan<int> sb{b, 3};

  auto s = arena.concatSpans<int>(sa, sb);
  REQUIRE(s.size() == 5);
  REQUIRE(s[0] == 1);
  REQUIRE(s[2] == 3);
  REQUIRE(s[4] == 5);
}

TEST_CASE("ASTArena: appendSpan adds one element", "[core][arena][concat]") {
  ASTArena arena;
  int arr[2] = {1, 2};
  ArenaSpan<int> base{arr, 2};

  auto s = arena.appendSpan<int>(base, 3);
  REQUIRE(s.size() == 3);
  REQUIRE(s[0] == 1);
  REQUIRE(s[2] == 3);
}

TEST_CASE("ASTArena: appendSpan adds an init list", "[core][arena][concat]") {
  ASTArena arena;
  int arr[1] = {1};
  ArenaSpan<int> base{arr, 1};

  auto s = arena.appendSpan<int>(base, {2, 3});
  REQUIRE(s.size() == 3);
  REQUIRE(s[2] == 3);
}

// ─────────────────────────────────────────────────────────────────────────────
// ASTArena: empty span helpers
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ASTArena: emptySpan is empty", "[core][arena]") {
  auto s = ASTArena::emptySpan<int>();
  REQUIRE(s.empty());
}