/// @file tests/parser/test_lexer.cpp
///
/// @brief Tests for the Lucid lexer.
///
/// ─── Numeric literals and the sign rule ──────────────────────────────────
/// The format has no operators. There is no MINUS token. A leading `-`
/// is part of the numeric literal, not a separate token.
///
///   -400.0   →  FLOAT_LITERAL("-400.0")
///   -42      →  INT_LITERAL("-42")
///
/// A `-` that is not immediately followed by a digit is a lexical error
/// (Lex_UnknownCharacter). This includes `- 42` (space between sign and
/// digits) and `-x` (sign before an identifier).
///
/// The line-comment opener `--` and the block-comment opener `/-` are
/// matched before the number lexer, so they are never confused with a
/// signed literal.
///
/// Table-driven. Each test feeds a source string to `tokenize` and checks
/// the resulting token types and, where relevant, the interned payload of
/// each token.

#include "parser/lexer/Lexer.hpp"

#include "core/Tokens.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/memory/StringPool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string_view>
#include <vector>

namespace
{

    // ─── Helpers ──────────────────────────────────────────────────────────────

    /// Tokenize a source string and return the tokens.
    std::vector<Token> lex(std::string_view source,
                           StringPool &pool,
                           lucid::diag::DiagnosticEngine &diag)
    {
        return lucid::lexer::tokenize(source, pool, diag);
    }

    /// Check that a token vector matches a list of expected token types.
    /// The final EOF_TOKEN is implicit: the helper strips it from the actual
    /// and does not require it in the expected list.
    bool typesMatch(const std::vector<Token> &actual,
                    std::initializer_list<TokenType> expected)
    {
        if (actual.size() != expected.size() + 1)
            return false;
        if (!actual.back().isEof())
            return false;

        size_t i = 0;
        for (TokenType t : expected)
        {
            if (actual[i].type != t)
                return false;
            ++i;
        }
        return true;
    }

    /// The payload of the token at `index`, as a string_view into the pool.
    std::string_view payload(const std::vector<Token> &toks,
                             StringPool &pool,
                             size_t index)
    {
        return pool.lookupView(toks[index].value);
    }

    /// Count the diagnostics with a given code.
    int countCode(const lucid::diag::DiagnosticEngine &diag,
                  lucid::diag::DiagCode code)
    {
        int n = 0;
        for (const auto &d : diag.all())
        {
            if (d.code == code)
                ++n;
        }
        return n;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Empty and trivial sources
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: empty source produces only EOF", "[parser][lexer]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("", pool, diag);

    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].isEof());
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: whitespace-only source produces only EOF",
          "[parser][lexer]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("   \t\r\n  \n\t", pool, diag);

    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].isEof());
    REQUIRE(diag.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Keywords
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: each keyword produces its token type",
          "[parser][lexer][keywords]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex(
        "import from enum resource node on",
        pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::KW_IMPORT,
                                 TokenType::KW_FROM,
                                 TokenType::KW_ENUM,
                                 TokenType::KW_RESOURCE,
                                 TokenType::KW_NODE,
                                 TokenType::KW_ON,
                             }));
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: `as` is an identifier, not a keyword",
          "[parser][lexer][keywords]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("as", pool, diag);

    REQUIRE(typesMatch(toks, {TokenType::IDENTIFIER}));
    REQUIRE(payload(toks, pool, 0) == "as");
}

TEST_CASE("Lexer: `emits` is an identifier (removed from grammar)",
          "[parser][lexer][keywords]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("emits", pool, diag);

    REQUIRE(typesMatch(toks, {TokenType::IDENTIFIER}));
    REQUIRE(payload(toks, pool, 0) == "emits");
}

// ─────────────────────────────────────────────────────────────────────────────
// Identifiers
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: simple identifiers", "[parser][lexer][identifiers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("foo bar Baz _x a1 _", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::IDENTIFIER,
                                 TokenType::IDENTIFIER,
                                 TokenType::IDENTIFIER,
                                 TokenType::IDENTIFIER,
                                 TokenType::IDENTIFIER,
                                 TokenType::IDENTIFIER,
                             }));
    REQUIRE(payload(toks, pool, 0) == "foo");
    REQUIRE(payload(toks, pool, 1) == "bar");
    REQUIRE(payload(toks, pool, 2) == "Baz");
    REQUIRE(payload(toks, pool, 3) == "_x");
    REQUIRE(payload(toks, pool, 4) == "a1");
    REQUIRE(payload(toks, pool, 5) == "_");
}

TEST_CASE("Lexer: identifiers are case-sensitive",
          "[parser][lexer][identifiers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("Node node NODE", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::IDENTIFIER,
                                 TokenType::KW_NODE,
                                 TokenType::IDENTIFIER,
                             }));
    REQUIRE(payload(toks, pool, 0) == "Node");
    REQUIRE(payload(toks, pool, 2) == "NODE");
}

TEST_CASE("Lexer: keywords cannot be used as identifiers",
          "[parser][lexer][identifiers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("import resource", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::KW_IMPORT,
                                 TokenType::KW_RESOURCE,
                             }));
}

// ─────────────────────────────────────────────────────────────────────────────
// Integer literals
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: decimal integers", "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("0 1 42 1000", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "0");
    REQUIRE(payload(toks, pool, 1) == "1");
    REQUIRE(payload(toks, pool, 2) == "42");
    REQUIRE(payload(toks, pool, 3) == "1000");
}

TEST_CASE("Lexer: hex integers", "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("0x0 0xFF 0xdeadbeef 0XABC", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "0x0");
    REQUIRE(payload(toks, pool, 1) == "0xFF");
    REQUIRE(payload(toks, pool, 2) == "0xdeadbeef");
    REQUIRE(payload(toks, pool, 3) == "0XABC");
}

TEST_CASE("Lexer: binary integers", "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("0b0 0b1 0b1010 0B11", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "0b0");
    REQUIRE(payload(toks, pool, 1) == "0b1");
    REQUIRE(payload(toks, pool, 2) == "0b1010");
    REQUIRE(payload(toks, pool, 3) == "0B11");
}

TEST_CASE("Lexer: octal integers", "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("0o0 0o7 0o17 0O777", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "0o0");
    REQUIRE(payload(toks, pool, 1) == "0o7");
    REQUIRE(payload(toks, pool, 2) == "0o17");
    REQUIRE(payload(toks, pool, 3) == "0O777");
}

TEST_CASE("Lexer: radix literal with no digits is an error",
          "[parser][lexer][numbers][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("0x 0b 0o", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::UNKNOWN,
                                 TokenType::UNKNOWN,
                                 TokenType::UNKNOWN,
                             }));
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_InvalidRadixLiteral) == 3);
}

// ─────────────────────────────────────────────────────────────────────────────
// Float literals
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: simple floats", "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("0.0 1.5 200.0 3.14159", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "0.0");
    REQUIRE(payload(toks, pool, 1) == "1.5");
    REQUIRE(payload(toks, pool, 2) == "200.0");
    REQUIRE(payload(toks, pool, 3) == "3.14159");
}

TEST_CASE("Lexer: floats with exponents", "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("1.0e9 1.0E-9 1.5e+3 2.0E10", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "1.0e9");
    REQUIRE(payload(toks, pool, 1) == "1.0E-9");
    REQUIRE(payload(toks, pool, 2) == "1.5e+3");
    REQUIRE(payload(toks, pool, 3) == "2.0E10");
}

TEST_CASE("Lexer: exponent with no digits is an error",
          "[parser][lexer][numbers][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("1.0e 2.0e+", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::UNKNOWN,
                                 TokenType::UNKNOWN,
                             }));
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_InvalidNumberLiteral) == 2);
}

TEST_CASE("Lexer: `.` after an integer without a following digit is DOT",
          "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("1.x", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::INT_LITERAL,
                                 TokenType::DOT,
                                 TokenType::IDENTIFIER,
                             }));
}

TEST_CASE("Lexer: `.5` and `1.` are not valid floats",
          "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto a = lex(".5", pool, diag);
    REQUIRE(typesMatch(a, {TokenType::DOT, TokenType::INT_LITERAL}));

    auto b = lex("1.", pool, diag);
    REQUIRE(typesMatch(b, {TokenType::INT_LITERAL, TokenType::DOT}));
}

// ─────────────────────────────────────────────────────────────────────────────
// Negative numbers
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: negative integers", "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("-0 -1 -42 -1000", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "-0");
    REQUIRE(payload(toks, pool, 1) == "-1");
    REQUIRE(payload(toks, pool, 2) == "-42");
    REQUIRE(payload(toks, pool, 3) == "-1000");
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: negative floats", "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("-0.0 -1.5 -200.0 -3.14159", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "-0.0");
    REQUIRE(payload(toks, pool, 1) == "-1.5");
    REQUIRE(payload(toks, pool, 2) == "-200.0");
    REQUIRE(payload(toks, pool, 3) == "-3.14159");
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: negative hex, binary, octal",
          "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("-0xFF -0b1010 -0o17", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                                 TokenType::INT_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "-0xFF");
    REQUIRE(payload(toks, pool, 1) == "-0b1010");
    REQUIRE(payload(toks, pool, 2) == "-0o17");
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: negative floats with exponents",
          "[parser][lexer][numbers]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("-1.0e9 -1.5e-3 -2.0E+10", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                                 TokenType::FLOAT_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "-1.0e9");
    REQUIRE(payload(toks, pool, 1) == "-1.5e-3");
    REQUIRE(payload(toks, pool, 2) == "-2.0E+10");
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: `-` not before a digit is an error",
          "[parser][lexer][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("a - b", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::IDENTIFIER,
                                 TokenType::UNKNOWN,
                                 TokenType::IDENTIFIER,
                             }));
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_UnknownCharacter) == 1);
}

TEST_CASE("Lexer: `-` with a space before the digit is an error",
          "[parser][lexer][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("- 42", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::UNKNOWN,
                                 TokenType::INT_LITERAL,
                             }));
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_UnknownCharacter) == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// String literals
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: simple strings", "[parser][lexer][strings]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("\"\" \"hello\" \"with spaces\"", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::STRING_LITERAL,
                                 TokenType::STRING_LITERAL,
                                 TokenType::STRING_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "");
    REQUIRE(payload(toks, pool, 1) == "hello");
    REQUIRE(payload(toks, pool, 2) == "with spaces");
}

TEST_CASE("Lexer: strings with escapes", "[parser][lexer][strings]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("\"a\\nb\\tc\\\\d\\\"e\"", pool, diag);

    REQUIRE(typesMatch(toks, {TokenType::STRING_LITERAL}));
    REQUIRE(payload(toks, pool, 0) == "a\nb\tc\\d\"e");
}

TEST_CASE("Lexer: unterminated string is an error",
          "[parser][lexer][strings][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("\"hello", pool, diag);

    REQUIRE(typesMatch(toks, {TokenType::UNKNOWN}));
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_UnterminatedString) == 1);
}

TEST_CASE("Lexer: string with a newline is an error",
          "[parser][lexer][strings][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("\"hello\nworld\"", pool, diag);

    REQUIRE(toks[0].type == TokenType::UNKNOWN);
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_NewlineInString) >= 1);
}

TEST_CASE("Lexer: unknown escape is an error",
          "[parser][lexer][strings][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("\"a\\qb\"", pool, diag);

    REQUIRE(typesMatch(toks, {TokenType::UNKNOWN}));
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_InvalidEscapeSequence) == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Char literals
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: simple char literals", "[parser][lexer][chars]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("'a' 'Z' '0' ' '", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::CHAR_LITERAL,
                                 TokenType::CHAR_LITERAL,
                                 TokenType::CHAR_LITERAL,
                                 TokenType::CHAR_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "a");
    REQUIRE(payload(toks, pool, 1) == "Z");
    REQUIRE(payload(toks, pool, 2) == "0");
    REQUIRE(payload(toks, pool, 3) == " ");
}

TEST_CASE("Lexer: char literals with escapes", "[parser][lexer][chars]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("'\\n' '\\t' '\\\\' '\\'' '\\0'", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::CHAR_LITERAL,
                                 TokenType::CHAR_LITERAL,
                                 TokenType::CHAR_LITERAL,
                                 TokenType::CHAR_LITERAL,
                                 TokenType::CHAR_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "\n");
    REQUIRE(payload(toks, pool, 1) == "\t");
    REQUIRE(payload(toks, pool, 2) == "\\");
    REQUIRE(payload(toks, pool, 3) == "'");
    REQUIRE(payload(toks, pool, 4) == std::string_view("\0", 1));
}

TEST_CASE("Lexer: unterminated char literal is an error",
          "[parser][lexer][chars][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("'a", pool, diag);

    REQUIRE(toks[0].type == TokenType::UNKNOWN);
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_UnterminatedCharLiteral) == 1);
}

TEST_CASE("Lexer: char literal with a newline is an error",
          "[parser][lexer][chars][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("'\n'", pool, diag);

    REQUIRE(toks[0].type == TokenType::UNKNOWN);
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_UnterminatedCharLiteral) == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Boolean and nil literals
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: true, false, nil are literals",
          "[parser][lexer][literals]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("true false nil", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::BOOL_LITERAL,
                                 TokenType::BOOL_LITERAL,
                                 TokenType::NIL_LITERAL,
                             }));
    REQUIRE(payload(toks, pool, 0) == "true");
    REQUIRE(payload(toks, pool, 1) == "false");
    REQUIRE(payload(toks, pool, 2) == "nil");
}

// ─────────────────────────────────────────────────────────────────────────────
// Punctuation
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: every punctuation mark", "[parser][lexer][punct]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("( ) { } [ ] , . : = @", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::LPAREN,
                                 TokenType::RPAREN,
                                 TokenType::LBRACE,
                                 TokenType::RBRACE,
                                 TokenType::LBRACKET,
                                 TokenType::RBRACKET,
                                 TokenType::COMMA,
                                 TokenType::DOT,
                                 TokenType::COLON,
                                 TokenType::EQUALS,
                                 TokenType::AT_SIGN,
                             }));
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: punctuation without surrounding whitespace",
          "[parser][lexer][punct]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("node x=NodeType(1,2)", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::KW_NODE,
                                 TokenType::IDENTIFIER,
                                 TokenType::EQUALS,
                                 TokenType::IDENTIFIER,
                                 TokenType::LPAREN,
                                 TokenType::INT_LITERAL,
                                 TokenType::COMMA,
                                 TokenType::INT_LITERAL,
                                 TokenType::RPAREN,
                             }));
}

// ─────────────────────────────────────────────────────────────────────────────
// Comments
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: line comments are dropped", "[parser][lexer][comments]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("node -- this is a comment\n", pool, diag);

    REQUIRE(typesMatch(toks, {TokenType::KW_NODE}));
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: multiple line comments", "[parser][lexer][comments]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex(
        "-- first\n"
        "node -- trailing\n"
        "-- third\n",
        pool, diag);

    REQUIRE(typesMatch(toks, {TokenType::KW_NODE}));
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: block comments are dropped",
          "[parser][lexer][comments]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("node /- a block comment -/ x", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::KW_NODE,
                                 TokenType::IDENTIFIER,
                             }));
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: block comments nest", "[parser][lexer][comments]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("a /- outer /- inner -/ still outer -/ b", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::IDENTIFIER,
                                 TokenType::IDENTIFIER,
                             }));
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: block comment containing `--` is not a line comment",
          "[parser][lexer][comments]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("a /- contains -- here -/ b", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::IDENTIFIER,
                                 TokenType::IDENTIFIER,
                             }));
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: unterminated block comment is an error",
          "[parser][lexer][comments][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("node /- never closed", pool, diag);

    REQUIRE(typesMatch(toks, {TokenType::KW_NODE}));
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_UnterminatedBlockComment) == 1);
}

// ─── Comment precedence over the sign ─────────────────────────────────────

TEST_CASE("Lexer: `--` starts a line comment, not a negative number",
          "[parser][lexer][comments]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("a --5\nb", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::IDENTIFIER,
                                 TokenType::IDENTIFIER,
                             }));
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: `/-` starts a block comment, not a negative number",
          "[parser][lexer][comments]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("a /- 5 -/ b", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::IDENTIFIER,
                                 TokenType::IDENTIFIER,
                             }));
    REQUIRE(diag.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Error recovery
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: unknown character produces UNKNOWN and continues",
          "[parser][lexer][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("a $ b # c", pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::IDENTIFIER,
                                 TokenType::UNKNOWN,
                                 TokenType::IDENTIFIER,
                                 TokenType::UNKNOWN,
                                 TokenType::IDENTIFIER,
                             }));
    REQUIRE(countCode(diag, lucid::diag::DiagCode::Lex_UnknownCharacter) == 2);
}

TEST_CASE("Lexer: errors do not prevent the rest of the file from lexing",
          "[parser][lexer][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex(
        "node good = IntNode(1)\n"
        "node bad  = IntNode($)\n"
        "node also_good = IntNode(2)\n",
        pool, diag);

    REQUIRE_FALSE(diag.empty());
    REQUIRE(toks.back().isEof());

    int nodeCount = 0;
    for (const auto &t : toks)
    {
        if (t.type == TokenType::KW_NODE)
            ++nodeCount;
    }
    REQUIRE(nodeCount == 3);
}

// ─────────────────────────────────────────────────────────────────────────────
// Location tracking
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: locations are 1-indexed", "[parser][lexer][location]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("a b c", pool, diag);

    REQUIRE(toks[0].location.line() == 1);
    REQUIRE(toks[0].location.column() == 1);
    REQUIRE(toks[1].location.line() == 1);
    REQUIRE(toks[1].location.column() == 3);
    REQUIRE(toks[2].location.line() == 1);
    REQUIRE(toks[2].location.column() == 5);
}

TEST_CASE("Lexer: locations advance across lines",
          "[parser][lexer][location]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("a\nbb\nccc", pool, diag);

    REQUIRE(toks[0].location.line() == 1);
    REQUIRE(toks[0].location.column() == 1);
    REQUIRE(toks[1].location.line() == 2);
    REQUIRE(toks[1].location.column() == 1);
    REQUIRE(toks[2].location.line() == 3);
    REQUIRE(toks[2].location.column() == 1);
}

TEST_CASE("Lexer: location of an unterminated string points at the quote",
          "[parser][lexer][location][error]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("node x = \"unterminated", pool, diag);

    REQUIRE_FALSE(diag.all().empty());
    const auto &d = diag.all().back();
    REQUIRE(d.location.column() == 10);
}

TEST_CASE("Lexer: location of a negative number starts at the sign",
          "[parser][lexer][location]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex("x = -42", pool, diag);

    // `-42` starts at column 5.
    REQUIRE(toks[2].type == TokenType::INT_LITERAL);
    REQUIRE(toks[2].location.column() == 5);
}

// ─────────────────────────────────────────────────────────────────────────────
// Integration
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Lexer: a full resource declaration lexes as expected",
          "[parser][lexer][integration]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex(
        "@export\n"
        "resource PlayerConfig {\n"
        "    speed:      float = 200.0\n"
        "    jump_force: float = -400.0\n"
        "    max_hp:     int   = 100\n"
        "    key_left:   Key   = Key.A\n"
        "}\n",
        pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::AT_SIGN,
                                 TokenType::IDENTIFIER, // export
                                 TokenType::KW_RESOURCE,
                                 TokenType::IDENTIFIER, // PlayerConfig
                                 TokenType::LBRACE,

                                 TokenType::IDENTIFIER, // speed
                                 TokenType::COLON,
                                 TokenType::IDENTIFIER, // float
                                 TokenType::EQUALS,
                                 TokenType::FLOAT_LITERAL, // 200.0

                                 TokenType::IDENTIFIER, // jump_force
                                 TokenType::COLON,
                                 TokenType::IDENTIFIER, // float
                                 TokenType::EQUALS,
                                 TokenType::FLOAT_LITERAL, // -400.0

                                 TokenType::IDENTIFIER, // max_hp
                                 TokenType::COLON,
                                 TokenType::IDENTIFIER, // int
                                 TokenType::EQUALS,
                                 TokenType::INT_LITERAL, // 100

                                 TokenType::IDENTIFIER, // key_left
                                 TokenType::COLON,
                                 TokenType::IDENTIFIER, // Key
                                 TokenType::EQUALS,
                                 TokenType::IDENTIFIER, // Key
                                 TokenType::DOT,
                                 TokenType::IDENTIFIER, // A

                                 TokenType::RBRACE,
                             }));
    REQUIRE(payload(toks, pool, 9) == "200.0");
    REQUIRE(payload(toks, pool, 14) == "-400.0");
    REQUIRE(diag.empty());
}

TEST_CASE("Lexer: a node declaration with an `on` clause lexes as expected",
          "[parser][lexer][integration]")
{
    StringPool pool;
    lucid::diag::DiagnosticEngine diag;

    auto toks = lex(
        "node play_sfx = PlaySound(\"hit.wav\") on on_hit, on_other",
        pool, diag);

    REQUIRE(typesMatch(toks, {
                                 TokenType::KW_NODE,
                                 TokenType::IDENTIFIER, // play_sfx
                                 TokenType::EQUALS,
                                 TokenType::IDENTIFIER, // PlaySound
                                 TokenType::LPAREN,
                                 TokenType::STRING_LITERAL, // "hit.wav"
                                 TokenType::RPAREN,
                                 TokenType::KW_ON,
                                 TokenType::IDENTIFIER, // on_hit
                                 TokenType::COMMA,
                                 TokenType::IDENTIFIER, // on_other
                             }));
    REQUIRE(diag.empty());
}
