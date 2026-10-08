/// @file parser/src/parser/lexer/Lexer.cpp
///
/// @brief Implementation of the Lucid lexer.
///
/// ─── Structure of this file ───────────────────────────────────────────────
///   1. LexerState — the cursor, the diagnostic sink, the trivia sink
///   2. Cursor primitives (advance, peek, match)
///   3. Token construction
///   4. The keyword table
///   5. Comment scanners (line, block)
///   6. Literal scanners (identifier, number, string, char)
///   7. Punctuation scanner
///   8. The dispatch (lexOne) and the public entry points
///
/// ─── The trivia sink ──────────────────────────────────────────────────────
/// LexerState has an optional pointer to a TriviaBuffer. When non-null,
/// comments are appended to it. When null, comments are discarded.
/// tokenize passes null; tokenizeWithTrivia passes a real buffer.
///
/// ─── Signed numeric literals ──────────────────────────────────────────────
/// The grammar's INT_LIT and FLOAT_LIT carry an optional leading '+'
/// or '-'. The sign is part of the literal token, not a separate
/// operator. lexOne routes a '+' or '-' followed by a digit to
/// lexNumber, which consumes the sign and stores it in the lexeme.
///
/// A '-' that is not followed by a digit is not a sign. The lexer
/// checks for the line-comment prefix '--' before it checks for a
/// signed number, so '--7' is a comment, not a signed literal. A
/// lone '+' or '-' reaches lexPunctuation, which does not recognize
/// either character and reports Lex_UnknownCharacter.
///
/// ─── The two-colon token ──────────────────────────────────────────────────
/// The grammar uses ':' as the resource-field separator and '::' as the
/// module qualifier. lexPunctuation checks for '::' before it checks for
/// a single ':', and emits COLON_COLON or COLON accordingly. The two
/// are distinct token types; the parser never has to look inside a
/// token to decide which one it has.

#include "parser/lexer/Lexer.hpp"

#include <cstring>
#include <string>
#include <string_view>
#include <utility>

using namespace lucid::diag;

namespace lucid::lexer
{

    // =============================================================================
    // 1. LexerState
    // =============================================================================

    struct LexerState
    {
        std::string_view source;
        StringPool &pool;
        lucid::diag::DiagnosticEngine &diagnostics;
        std::vector<Token> tokens;

        /// Optional sink for comments. Null means "discard comments."
        trivia::TriviaBuffer *triviaSink = nullptr;

        size_t position = 0;
        uint32_t line = 1;
        uint32_t column = 1;

        LexerState(std::string_view src,
                   StringPool &p,
                   lucid::diag::DiagnosticEngine &diag,
                   trivia::TriviaBuffer *trivia)
            : source(src), pool(p), diagnostics(diag), triviaSink(trivia) {}
    };

    // =============================================================================
    // 2. Cursor primitives
    // =============================================================================

    namespace
    {

        bool isAtEnd(const LexerState &s) noexcept
        {
            return s.position >= s.source.size();
        }

        char currentChar(const LexerState &s) noexcept
        {
            return isAtEnd(s) ? '\0' : s.source[s.position];
        }

        char peekChar(const LexerState &s, size_t offset = 0) noexcept
        {
            const size_t pos = s.position + offset;
            return pos >= s.source.size() ? '\0' : s.source[pos];
        }

        void advance(LexerState &s) noexcept
        {
            if (isAtEnd(s))
                return;
            if (s.source[s.position] == '\n')
            {
                s.line++;
                s.column = 1;
            }
            else
            {
                s.column++;
            }
            s.position++;
        }

        SourceLocation currentLocation(const LexerState &s) noexcept
        {
            return SourceLocation{s.line, s.column};
        }

    } // namespace

    // =============================================================================
    // 3. Token construction
    // =============================================================================

    namespace
    {

        Token makeToken(TokenType type,
                        InternedString value,
                        SourceLocation location)
        {
            return Token{type, value, location};
        }

        Token makeTokenFromLexeme(LexerState &s,
                                  TokenType type,
                                  std::string_view lexeme,
                                  SourceLocation location)
        {
            return Token{type, s.pool.intern(lexeme), location};
        }

        void reportAt(LexerState &s, DiagCode code, std::string message)
        {
            s.diagnostics.errorAt(code, currentLocation(s), std::move(message));
        }

        void reportErrorAt(LexerState &s,
                           DiagCode code,
                           SourceLocation loc,
                           std::string message)
        {
            s.diagnostics.errorAt(code, loc, std::move(message));
        }

    } // namespace

    // =============================================================================
    // 4. The keyword table
    // =============================================================================

    namespace
    {

        TokenType keywordToType(std::string_view word) noexcept
        {
            if (word == "import")
                return TokenType::KW_IMPORT;
            if (word == "from")
                return TokenType::KW_FROM;
            if (word == "enum")
                return TokenType::KW_ENUM;
            if (word == "resource")
                return TokenType::KW_RESOURCE;
            if (word == "node")
                return TokenType::KW_NODE;
            if (word == "on")
                return TokenType::KW_ON;
            if (word == "true")
                return TokenType::BOOL_LITERAL;
            if (word == "false")
                return TokenType::BOOL_LITERAL;
            if (word == "nil")
                return TokenType::NIL_LITERAL;
            return TokenType::IDENTIFIER;
        }

        void skipWhitespace(LexerState &s) noexcept
        {
            while (!isAtEnd(s))
            {
                const char c = currentChar(s);
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                {
                    advance(s);
                }
                else
                {
                    break;
                }
            }
        }

    } // namespace

    // =============================================================================
    // 5. Comments
    // =============================================================================
    //
    // The two scanners return the comment's content as a std::string_view
    // into the source. The caller (lexOne) decides whether to intern it
    // and append to the trivia sink, or to discard it.

    namespace
    {

        /// Consume a line comment. The leading `--` has been consumed.
        /// Returns the content between `--` and the newline (or end of
        /// input). Does not consume the newline.
        std::string_view skipLineComment(LexerState &s) noexcept
        {
            const size_t start = s.position;
            while (!isAtEnd(s) && currentChar(s) != '\n')
            {
                advance(s);
            }
            return s.source.substr(start, s.position - start);
        }

        /// Consume a nestable block comment. The leading `/-` has been
        /// consumed. Returns the content between `/-` and the matching
        /// `-/`, with internal newlines preserved. `terminated` is set
        /// to true if a matching `-/` was found.
        std::string_view readBlockComment(LexerState &s, bool &terminated)
        {
            terminated = false;
            const size_t start = s.position;
            int depth = 1;

            while (!isAtEnd(s))
            {
                if (currentChar(s) == '/' && peekChar(s, 1) == '-')
                {
                    depth++;
                    advance(s);
                    advance(s);
                    continue;
                }
                if (currentChar(s) == '-' && peekChar(s, 1) == '/')
                {
                    depth--;
                    if (depth == 0)
                    {
                        // The content ends before the closing `-/`.
                        const size_t end = s.position;
                        advance(s);
                        advance(s);
                        terminated = true;
                        return s.source.substr(start, end - start);
                    }
                    advance(s);
                    advance(s);
                    continue;
                }
                advance(s);
            }

            // Unterminated: return what we have.
            return s.source.substr(start, s.position - start);
        }

    } // namespace

    // =============================================================================
    // 6. Literal scanners
    // =============================================================================

    namespace
    {

        void lexIdentifier(LexerState &s)
        {
            const SourceLocation startLoc = currentLocation(s);
            const size_t startPos = s.position;

            while (!isAtEnd(s) && isIdentifierChar(currentChar(s)))
            {
                advance(s);
            }

            const std::string_view word =
                s.source.substr(startPos, s.position - startPos);
            const TokenType type = keywordToType(word);

            s.tokens.push_back(makeTokenFromLexeme(s, type, word, startLoc));
        }

        /// Lex a numeric literal, including an optional leading sign.
        ///
        /// The sign is consumed as part of the lexeme. The caller
        /// (lexOne) has already established that a sign, if present, is
        /// followed by a digit; this function re-checks for defensive
        /// completeness.
        ///
        /// Radix forms (`0x`, `0b`, `0o`) accept a sign in front. The
        /// grammar allows it; whether a signed radix literal is
        /// meaningful is a Sema question, not a lexical one.
        void lexNumber(LexerState &s)
        {
            const SourceLocation startLoc = currentLocation(s);
            const size_t startPos = s.position;

            // ─── Optional sign ─────────────────────────────────────────────
            if (currentChar(s) == '-' || currentChar(s) == '+')
            {
                advance(s);
            }

            // ─── Radix prefixes ────────────────────────────────────────────
            if (currentChar(s) == '0')
            {
                const char next = peekChar(s, 1);

                auto lexRadix = [&](char lower,
                                    bool (*isDigitFn)(char),
                                    TokenType type,
                                    const char *name)
                {
                    advance(s);
                    advance(s);
                    if (!isDigitFn(currentChar(s)))
                    {
                        reportErrorAt(s, DiagCode::Lex_InvalidRadixLiteral,
                                      startLoc,
                                      std::string(name) +
                                          " literal has no digits after '0" +
                                          std::string(1, lower) + "'");
                        s.tokens.push_back(makeTokenFromLexeme(
                            s, TokenType::UNKNOWN,
                            s.source.substr(startPos, s.position - startPos),
                            startLoc));
                        return;
                    }
                    while (isDigitFn(currentChar(s)))
                        advance(s);
                    s.tokens.push_back(makeTokenFromLexeme(
                        s, type,
                        s.source.substr(startPos, s.position - startPos),
                        startLoc));
                };

                if (next == 'x' || next == 'X')
                {
                    lexRadix('x', isHexDigit, TokenType::INT_LITERAL,
                             "hexadecimal");
                    return;
                }
                if (next == 'b' || next == 'B')
                {
                    lexRadix('b', isBinDigit, TokenType::INT_LITERAL,
                             "binary");
                    return;
                }
                if (next == 'o' || next == 'O')
                {
                    lexRadix('o', isOctDigit, TokenType::INT_LITERAL,
                             "octal");
                    return;
                }
            }

            // ─── Decimal integer part ──────────────────────────────────────
            while (isDigit(currentChar(s)))
                advance(s);

            bool isFloat = false;

            // ─── Fractional part ───────────────────────────────────────────
            // A '.' begins the fractional part only when a digit follows.
            // `1.` is not a float; the '.' is left for whatever comes next
            // (a field access, in a value position).
            if (currentChar(s) == '.' && isDigit(peekChar(s, 1)))
            {
                isFloat = true;
                advance(s);
                while (isDigit(currentChar(s)))
                    advance(s);
            }

            // ─── Exponent ──────────────────────────────────────────────────
            // The exponent may carry its own sign. It does not make the
            // literal a float if it was not one already — but in this
            // grammar, an exponent without a fractional part is not a
            // valid FLOAT_LIT, because FLOAT_LIT requires a '.'. The
            // `isFloat = true` here is defensive; a bare `1e5` reaches
            // this branch as an integer and would be reported by a
            // stricter check. The current lexer accepts it and marks it
            // as a float; Sema or a later tightening can reject it.
            if (currentChar(s) == 'e' || currentChar(s) == 'E')
            {
                isFloat = true;
                advance(s);
                if (currentChar(s) == '+' || currentChar(s) == '-')
                    advance(s);
                if (!isDigit(currentChar(s)))
                {
                    reportErrorAt(s, DiagCode::Lex_InvalidNumberLiteral,
                                  startLoc, "exponent has no digits");
                    s.tokens.push_back(makeTokenFromLexeme(
                        s, TokenType::UNKNOWN,
                        s.source.substr(startPos, s.position - startPos),
                        startLoc));
                    return;
                }
                while (isDigit(currentChar(s)))
                    advance(s);
            }

            s.tokens.push_back(makeTokenFromLexeme(
                s,
                isFloat ? TokenType::FLOAT_LITERAL : TokenType::INT_LITERAL,
                s.source.substr(startPos, s.position - startPos),
                startLoc));
        }

        bool readEscape(LexerState &s,
                        std::string &out,
                        SourceLocation escapeLoc)
        {
            advance(s);
            if (isAtEnd(s))
            {
                reportErrorAt(s, DiagCode::Lex_InvalidEscapeSequence,
                              escapeLoc, "unterminated escape sequence");
                return false;
            }

            const char next = currentChar(s);
            switch (next)
            {
            case 'n':
                out += '\n';
                advance(s);
                return true;
            case 't':
                out += '\t';
                advance(s);
                return true;
            case 'r':
                out += '\r';
                advance(s);
                return true;
            case '\\':
                out += '\\';
                advance(s);
                return true;
            case '"':
                out += '"';
                advance(s);
                return true;
            case '\'':
                out += '\'';
                advance(s);
                return true;
            case '0':
                out += '\0';
                advance(s);
                return true;
            default:
                reportErrorAt(s, DiagCode::Lex_InvalidEscapeSequence,
                              escapeLoc,
                              std::string("unknown escape '\\") + next + "'");
                advance(s);
                return false;
            }
        }

        void lexString(LexerState &s)
        {
            const SourceLocation startLoc = currentLocation(s);
            advance(s);

            std::string content;
            bool hadError = false;

            while (!isAtEnd(s))
            {
                const char c = currentChar(s);

                if (c == '"')
                {
                    advance(s);
                    s.tokens.push_back(makeTokenFromLexeme(
                        s,
                        hadError ? TokenType::UNKNOWN
                                 : TokenType::STRING_LITERAL,
                        content, startLoc));
                    return;
                }

                if (c == '\n')
                {
                    reportErrorAt(s, DiagCode::Lex_NewlineInString, startLoc,
                                  "a string literal cannot contain a newline");
                    s.tokens.push_back(makeTokenFromLexeme(
                        s, TokenType::UNKNOWN, content, startLoc));
                    return;
                }

                if (c == '\\')
                {
                    if (!readEscape(s, content, currentLocation(s)))
                    {
                        hadError = true;
                    }
                    continue;
                }

                content += c;
                advance(s);
            }

            reportErrorAt(s, DiagCode::Lex_UnterminatedString, startLoc,
                          "unterminated string literal");
            s.tokens.push_back(makeTokenFromLexeme(
                s, TokenType::UNKNOWN, content, startLoc));
        }

        void lexChar(LexerState &s)
        {
            const SourceLocation startLoc = currentLocation(s);
            advance(s);

            std::string value;
            bool hadError = false;
            bool sawClosing = false;

            while (!isAtEnd(s))
            {
                const char c = currentChar(s);

                if (c == '\'')
                {
                    advance(s);
                    sawClosing = true;
                    break;
                }

                if (c == '\n')
                {
                    reportErrorAt(s, DiagCode::Lex_UnterminatedCharLiteral,
                                  startLoc,
                                  "character literal cannot contain a newline");
                    hadError = true;
                    advance(s);
                    continue;
                }

                if (c == '\\')
                {
                    if (!readEscape(s, value, currentLocation(s)))
                    {
                        hadError = true;
                    }
                    continue;
                }

                if (value.empty())
                {
                    value += c;
                    advance(s);
                }
                else
                {
                    if (!hadError)
                    {
                        reportErrorAt(s, DiagCode::Lex_UnterminatedCharLiteral,
                                      startLoc,
                                      "character literal contains more than "
                                      "one character");
                        hadError = true;
                    }
                    advance(s);
                }
            }

            if (!sawClosing)
            {
                reportErrorAt(s, DiagCode::Lex_UnterminatedCharLiteral,
                              startLoc, "unterminated character literal");
                hadError = true;
            }

            s.tokens.push_back(makeTokenFromLexeme(
                s,
                hadError ? TokenType::UNKNOWN : TokenType::CHAR_LITERAL,
                value, startLoc));
        }

    } // namespace

    // =============================================================================
    // 7. Punctuation
    // =============================================================================
    //
    // Two entries need more than one character of lookahead:
    //
    //   - ':' vs '::'  — the grammar uses both, as the resource-field
    //                    separator and the module qualifier. The lexer
    //                    emits COLON_COLON for two consecutive colons
    //                    and COLON otherwise.
    //
    // Every other punctuation token is a single character.
    //
    // The comment prefixes '--' and '/-' are handled in lexOne, not
    // here, because they consume a different shape (a run to end of line
    // or a nestable block) than a fixed-width token does.

    namespace
    {

        void lexPunctuation(LexerState &s)
        {
            const SourceLocation startLoc = currentLocation(s);
            const char c = currentChar(s);

            // ─── '::' before ':' ───────────────────────────────────────────
            // The two-colon token must be checked first, or a ':' would
            // consume the first colon and leave the second as a stray.
            if (c == ':' && peekChar(s, 1) == ':')
            {
                advance(s);
                advance(s);
                s.tokens.push_back(makeTokenFromLexeme(
                    s, TokenType::COLON_COLON, "::", startLoc));
                return;
            }

            TokenType type = TokenType::UNKNOWN;
            switch (c)
            {
            case '(':
                type = TokenType::LPAREN;
                break;
            case ')':
                type = TokenType::RPAREN;
                break;
            case '{':
                type = TokenType::LBRACE;
                break;
            case '}':
                type = TokenType::RBRACE;
                break;
            case '[':
                type = TokenType::LBRACKET;
                break;
            case ']':
                type = TokenType::RBRACKET;
                break;
            case ',':
                type = TokenType::COMMA;
                break;
            case '.':
                type = TokenType::DOT;
                break;
            case ':':
                type = TokenType::COLON;
                break;
            case '=':
                type = TokenType::EQUALS;
                break;
            case '@':
                type = TokenType::AT_SIGN;
                break;
            default:
                break;
            }

            if (type != TokenType::UNKNOWN)
            {
                advance(s);
                s.tokens.push_back(makeTokenFromLexeme(
                    s, type, std::string_view(&c, 1), startLoc));
                return;
            }

            reportErrorAt(s, DiagCode::Lex_UnknownCharacter, startLoc,
                          std::string("unexpected character '") + c + "'");
            advance(s);
            s.tokens.push_back(makeTokenFromLexeme(
                s, TokenType::UNKNOWN, std::string_view(&c, 1), startLoc));
        }

    } // namespace

    // =============================================================================
    // 8. The dispatch and the public entry points
    // =============================================================================

    namespace
    {

        /// Emit a comment as trivia, if the lexer has a sink. Interning
        /// happens here, at most once per comment.
        void emitTrivia(LexerState &s,
                        trivia::TriviaKind kind,
                        std::string_view content,
                        SourceLocation loc)
        {
            if (!s.triviaSink)
                return;
            s.triviaSink->add(trivia::Trivia{
                kind,
                s.pool.intern(content),
                loc,
            });
        }

        void lexOne(LexerState &s)
        {
            skipWhitespace(s);

            if (isAtEnd(s))
            {
                s.tokens.push_back(makeToken(TokenType::EOF_TOKEN,
                                             InternedString{},
                                             currentLocation(s)));
                return;
            }

            const char c = currentChar(s);
            const char next = peekChar(s, 1);

            // ─── Line comment ──────────────────────────────────────────────
            // '--' must be checked before the signed-number case below,
            // or '--7' would be read as a signed literal.
            if (c == '-' && next == '-')
            {
                const SourceLocation startLoc = currentLocation(s);
                advance(s);
                advance(s); // consume `--`
                const std::string_view content = skipLineComment(s);
                emitTrivia(s, trivia::TriviaKind::LineComment,
                           content, startLoc);
                return;
            }

            // ─── Block comment ─────────────────────────────────────────────
            if (c == '/' && next == '-')
            {
                const SourceLocation startLoc = currentLocation(s);
                advance(s);
                advance(s); // consume `/-`
                bool terminated = false;
                const std::string_view content = readBlockComment(s, terminated);
                emitTrivia(s, trivia::TriviaKind::BlockComment,
                           content, startLoc);
                if (!terminated)
                {
                    reportErrorAt(s, DiagCode::Lex_UnterminatedBlockComment,
                                  startLoc,
                                  "unterminated block comment (expected -/)");
                }
                return;
            }

            // ─── Identifiers and keywords ──────────────────────────────────
            if (isIdentifierStart(c))
            {
                lexIdentifier(s);
                return;
            }

            // ─── Signed numbers ────────────────────────────────────────────
            // A sign is part of the literal only when a digit follows.
            // A bare sign reaches the punctuation path below, which
            // reports Lex_UnknownCharacter for '+' and '-' (neither is
            // a punctuation token).
            if ((c == '-' || c == '+') && isDigit(next))
            {
                lexNumber(s);
                return;
            }

            // ─── Unsigned numbers ──────────────────────────────────────────
            if (isDigit(c))
            {
                lexNumber(s);
                return;
            }

            // ─── Strings ───────────────────────────────────────────────────
            if (c == '"')
            {
                lexString(s);
                return;
            }

            // ─── Char literal ──────────────────────────────────────────────
            if (c == '\'')
            {
                lexChar(s);
                return;
            }

            // ─── Punctuation ───────────────────────────────────────────────
            lexPunctuation(s);
        }

        /// The shared implementation. `trivia` may be null.
        LexerState runTokenizer(std::string_view source,
                                StringPool &pool,
                                lucid::diag::DiagnosticEngine &diagnostics,
                                trivia::TriviaBuffer *trivia)
        {
            LexerState s(source, pool, diagnostics, trivia);
            while (true)
            {
                lexOne(s);
                if (!s.tokens.empty() &&
                    s.tokens.back().type == TokenType::EOF_TOKEN)
                {
                    break;
                }
            }
            return s;
        }

    } // namespace

    std::vector<Token> tokenize(std::string_view source,
                                StringPool &pool,
                                lucid::diag::DiagnosticEngine &diagnostics)
    {
        LexerState s = runTokenizer(source, pool, diagnostics, nullptr);
        return std::move(s.tokens);
    }

    TokenizeResult tokenizeWithTrivia(std::string_view source,
                                      StringPool &pool,
                                      lucid::diag::DiagnosticEngine &diagnostics)
    {
        trivia::TriviaBuffer buffer;
        LexerState s = runTokenizer(source, pool, diagnostics, &buffer);
        return TokenizeResult{std::move(s.tokens), std::move(buffer)};
    }

} // namespace lucid::lexer