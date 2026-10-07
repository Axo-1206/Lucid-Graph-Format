/// @file tests/core/test_trivia.cpp
///
/// @brief Tests for Trivia and TriviaBuffer.

#include "core/trivia/Trivia.hpp"
#include "core/trivia/TriviaBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

using lucid::trivia::Trivia;
using lucid::trivia::TriviaBuffer;
using lucid::trivia::TriviaKind;

TEST_CASE("TriviaBuffer is empty by default", "[trivia]")
{
    TriviaBuffer buf;
    CHECK(buf.empty());
    CHECK(buf.size() == 0);
}

TEST_CASE("TriviaBuffer::add appends in order", "[trivia]")
{
    TriviaBuffer buf;
    buf.add(Trivia{TriviaKind::LineComment, InternedString{1}, {1, 1}});
    buf.add(Trivia{TriviaKind::LineComment, InternedString{2}, {2, 1}});
    buf.add(Trivia{TriviaKind::BlockComment, InternedString{3}, {3, 1}});

    REQUIRE(buf.size() == 3);
    CHECK(buf[0].kind == TriviaKind::LineComment);
    CHECK(buf[1].kind == TriviaKind::LineComment);
    CHECK(buf[2].kind == TriviaKind::BlockComment);
}

TEST_CASE("TriviaBuffer::lowerBound on an empty buffer", "[trivia]")
{
    TriviaBuffer buf;
    CHECK(buf.lowerBound(SourceLocation{1, 1}) == 0);
    CHECK(buf.lowerBound(SourceLocation{100, 1}) == 0);
}

TEST_CASE("TriviaBuffer::lowerBound returns 0 for a location before all entries",
          "[trivia]")
{
    TriviaBuffer buf;
    buf.add(Trivia{TriviaKind::LineComment, InternedString{1}, {5, 1}});
    buf.add(Trivia{TriviaKind::LineComment, InternedString{2}, {10, 1}});

    CHECK(buf.lowerBound(SourceLocation{1, 1}) == 0);
    CHECK(buf.lowerBound(SourceLocation{5, 1}) == 0); // at-or-after
}

TEST_CASE("TriviaBuffer::lowerBound returns size for a location after all entries",
          "[trivia]")
{
    TriviaBuffer buf;
    buf.add(Trivia{TriviaKind::LineComment, InternedString{1}, {5, 1}});
    buf.add(Trivia{TriviaKind::LineComment, InternedString{2}, {10, 1}});

    CHECK(buf.lowerBound(SourceLocation{100, 1}) == 2);
    CHECK(buf.lowerBound(SourceLocation{11, 1}) == 2);
}

TEST_CASE("TriviaBuffer::lowerBound finds the exact match", "[trivia]")
{
    TriviaBuffer buf;
    buf.add(Trivia{TriviaKind::LineComment, InternedString{1}, {1, 1}});
    buf.add(Trivia{TriviaKind::LineComment, InternedString{2}, {5, 1}});
    buf.add(Trivia{TriviaKind::LineComment, InternedString{3}, {10, 1}});

    CHECK(buf.lowerBound(SourceLocation{1, 1}) == 0);
    CHECK(buf.lowerBound(SourceLocation{5, 1}) == 1);
    CHECK(buf.lowerBound(SourceLocation{10, 1}) == 2);
}

TEST_CASE("TriviaBuffer::lowerBound handles a middle location", "[trivia]")
{
    TriviaBuffer buf;
    buf.add(Trivia{TriviaKind::LineComment, InternedString{1}, {1, 1}});
    buf.add(Trivia{TriviaKind::LineComment, InternedString{2}, {5, 1}});
    buf.add(Trivia{TriviaKind::LineComment, InternedString{3}, {10, 1}});

    CHECK(buf.lowerBound(SourceLocation{3, 1}) == 1); // between 1 and 5
    CHECK(buf.lowerBound(SourceLocation{7, 1}) == 2); // between 5 and 10
}

TEST_CASE("TriviaBuffer::lowerBound distinguishes columns on the same line",
          "[trivia]")
{
    TriviaBuffer buf;
    buf.add(Trivia{TriviaKind::LineComment, InternedString{1}, {1, 1}});
    buf.add(Trivia{TriviaKind::LineComment, InternedString{2}, {1, 10}});

    CHECK(buf.lowerBound(SourceLocation{1, 1}) == 0);
    CHECK(buf.lowerBound(SourceLocation{1, 5}) == 1); // between (1,1) and (1,10)
    CHECK(buf.lowerBound(SourceLocation{1, 10}) == 1);
    CHECK(buf.lowerBound(SourceLocation{1, 11}) == 2);
}

TEST_CASE("TriviaBuffer is copyable", "[trivia]")
{
    TriviaBuffer buf;
    buf.add(Trivia{TriviaKind::LineComment, InternedString{1}, {1, 1}});
    buf.add(Trivia{TriviaKind::BlockComment, InternedString{2}, {2, 1}});

    TriviaBuffer copy = buf;
    CHECK(copy.size() == buf.size());
    CHECK(copy[0].kind == buf[0].kind);
    CHECK(copy[1].kind == buf[1].kind);
}