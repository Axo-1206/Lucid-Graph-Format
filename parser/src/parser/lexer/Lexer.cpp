/**
 * @file parser/lexer/Lexer.cpp
 *
 * @brief Implementation of the Lucid lexer.
 *
 * ─── Structure of this file ───────────────────────────────────────────────
 *   1. LexerState — the cursor and the diagnostic sink
 *   2. Cursor primitives (advance, peek, match)
 *   3. Token construction
 *   4. The keyword table
 *   5. Comment scanners (line, block)
 *   6. Literal scanners (identifier, number, string, char)
 *   7. Punctuation scanner
 *   8. The dispatch (lexOne) and the public entry point
 *
 * ─── The cursor ───────────────────────────────────────────────────────────
 * The lexer maintains (position, line, column). Only `advance()` moves
 * them, and it moves all three together so they can never disagree. Every
 * token constructor captures the cursor's location *before* the token's
 * first character is consumed.
 */

#include "parser/Lexer.hpp"

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

    /// @brief The cursor, the pool, and the diagnostic sink.
    struct LexerState
    {
        std::string_view source;
        StringPool &pool;
        lucid::diag::DiagnosticEngine &diagnostics;
        std::vector<Token> tokens;

        size_t position = 0; // byte offset into source
        uint32_t line = 1;   // 1-indexed
        uint32_t column = 1; // 1-indexed

        LexerState(std::string_view src,
                   StringPool &p,
                   lucid::diag::DiagnosticEngine &diag)
            : source(src), pool(p), diagnostics(diag) {}
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

        /// @brief Build a token. The payload is already interned.
        Token makeToken(TokenType type,
                        InternedString value,
                        SourceLocation location)
        {
            return Token{type, value, location};
        }

        /// @brief Build a token from a raw lexeme, interning it through the pool.
        Token makeTokenFromLexeme(LexerState &s,
                                  TokenType type,
                                  std::string_view lexeme,
                                  SourceLocation location)
        {
            return Token{type, s.pool.intern(lexeme), location};
        }

        // ─── Diagnostics ──────────────────────────────────────────────────────────
        //
        // Every diagnostic takes an explicit location. The two helpers below exist
        // so the call sites read cleanly:
        //
        //   reportAt — the diagnostic is about the current cursor position.
        //   reportErrorAt — the diagnostic is about a location captured earlier
        //                   (the opening quote of an unterminated string, for
        //                   instance).

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
    //
    // This is the entire set of words the lexer recognizes as anything other
    // than IDENTIFIER. It has nine entries. Every entry corresponds to a KW_*
    // value in Tokens.hpp.
    //
    // A linear scan over string_views. Nine entries, one comparison per entry
    // on a miss; the branch predictor handles it well because most identifiers
    // share prefixes with at most a few keywords. If this ever shows up in a
    // profile, the fix is a perfect hash — not now.
    //
    // `as` is deliberately absent: the grammar's §1.2 does not list it, and
    // §5's lexer sketch omits it. The parser matches it by spelling. If that
    // decision changes, this is the one table to edit.

    namespace
    {

        TokenType keywordToType(std::string_view word) noexcept
        {
            // ─── Declaration keywords ───────────────────────────────────────────
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
            if (word == "composite")
                return TokenType::KW_COMPOSITE;

            // ─── Composite-body keywords ────────────────────────────────────────
            if (word == "on")
                return TokenType::KW_ON;
            if (word == "input")
                return TokenType::KW_INPUT;
            if (word == "output")
                return TokenType::KW_OUTPUT;

            // ─── Literal keywords ───────────────────────────────────────────────
            // `true` and `false` both produce BOOL_LITERAL; the token's payload
            // distinguishes them. `nil` produces NIL_LITERAL.
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
    // Two forms:
    //
    //   -- ...           line comment; dropped, no token.
    //   /- ... -/        block comment; nestable; dropped, no token.
    //
    // The block-comment opener is /- (two chars); its closer is -/ (two). A
    // block comment nests: a nested /- ... -/ inside it must be consumed
    // before the outer -/ can close it.
    //
    // There is no separate doc-comment form. `/--` is `/-` followed by `-`;
    // the block-comment scanner sees the opener, then a literal `-`. That is
    // consistent with the grammar, which lists only line and block comments.

    namespace
    {

        /// @brief Consume a line comment. The leading `--` has been consumed.
        void skipLineComment(LexerState &s) noexcept
        {
            while (!isAtEnd(s) && currentChar(s) != '\n')
                advance(s);
        }

        /// @brief Consume a nestable block comment. The leading `/-` has been consumed.
        /// @param terminated  Set to true if a matching `-/` was found.
        void readBlockComment(LexerState &s, bool &terminated)
        {
            terminated = false;
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
                    advance(s);
                    advance(s);
                    if (depth == 0)
                    {
                        terminated = true;
                        return;
                    }
                    continue;
                }
                advance(s);
            }
        }

    } // namespace

    // =============================================================================
    // 6. Literal scanners
    // =============================================================================

    namespace
    {

        // ─── Identifiers and keywords ─────────────────────────────────────────────

        /// @brief Lex an identifier or a keyword.
        ///
        /// The lexeme is interned once, whether the token is a keyword or an
        /// identifier. Keyword spellings are canonical strings ("node", "on"), so
        /// the pool ends up with one ID per keyword regardless of how many times
        /// each appears.
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

        // ─── Numbers ──────────────────────────────────────────────────────────────
        //
        // The lexer produces raw lexemes; it does not parse the number. `0xFF` is
        // a INT_LITERAL whose value is the interned string "0xFF". The parser or
        // Sema interprets the lexeme.
        //
        // A numeric literal may have a leading `-`. The sign is part of the token.
        // There is no MINUS token and no unary-minus parse rule: the format has
        // no operators. A `-` that is not immediately followed by a digit is not
        // part of a number and is reported as an unknown character.
        //
        // The `.` disambiguation: the number lexer is entered only when the
        // first character is a digit or a `-` followed by a digit. A `.` is
        // always DOT, even when followed by a digit: the grammar's FLOAT_LIT
        // requires a digit before the `.`. Inside the number lexer, a `.`
        // is part of a float only when the next character is a digit.

        void lexNumber(LexerState &s)
        {
            const SourceLocation startLoc = currentLocation(s);
            const size_t startPos = s.position;

            // Optional leading sign. A `-` reaching here is always followed by
            // a digit — the dispatch in lexOne only routes `-` to lexNumber when
            // the next character is a digit. The sign is captured in the lexeme
            // because the lexeme is taken from startPos to s.position.
            if (currentChar(s) == '-')
            {
                advance(s);
            }

            // Radix prefixes: 0x, 0b, 0o (case-insensitive).
            if (currentChar(s) == '0')
            {
                const char next = peekChar(s, 1);

                auto lexRadix = [&](char lower,
                                    bool (*isDigitFn)(char),
                                    TokenType type,
                                    const char *name)
                {
                    advance(s);
                    advance(s); // consume `0x` / `0b` / `0o`
                    if (!isDigitFn(currentChar(s)))
                    {
                        reportErrorAt(s, DiagCode::Lex_InvalidRadixLiteral, startLoc,
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
                    lexRadix('x', isHexDigit, TokenType::INT_LITERAL, "hexadecimal");
                    return;
                }
                if (next == 'b' || next == 'B')
                {
                    lexRadix('b', isBinDigit, TokenType::INT_LITERAL, "binary");
                    return;
                }
                if (next == 'o' || next == 'O')
                {
                    lexRadix('o', isOctDigit, TokenType::INT_LITERAL, "octal");
                    return;
                }
            }

            // Decimal integer part.
            while (isDigit(currentChar(s)))
                advance(s);

            bool isFloat = false;

            // Fractional part: `.` followed by a digit. A bare `.` is not part of
            // the number — `1.field` lexes as INT_LITERAL(1), DOT, IDENTIFIER.
            if (currentChar(s) == '.' && isDigit(peekChar(s, 1)))
            {
                isFloat = true;
                advance(s); // `.`
                while (isDigit(currentChar(s)))
                    advance(s);
            }

            // Exponent part.
            if (currentChar(s) == 'e' || currentChar(s) == 'E')
            {
                isFloat = true;
                advance(s);
                if (currentChar(s) == '+' || currentChar(s) == '-')
                    advance(s);
                if (!isDigit(currentChar(s)))
                {
                    reportErrorAt(s, DiagCode::Lex_InvalidNumberLiteral, startLoc,
                                  "exponent has no digits");
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

        // ─── Escape processing ────────────────────────────────────────────────────
        //
        // One function, shared by the string lexer and the char lexer. Resolves
        // the escape sequence to its actual character. The two callers diverge in
        // only one way: a string may contain a NUL (`'\0'`), which it appends to
        // its content; a char whose value is 0 is a legal char whose value is 0.
        // Both callers use the same resolved byte.
        //
        // On an invalid escape, the function reports the diagnostic, consumes
        // the offending character, and returns false. The caller decides
        // whether to bail out of the literal or continue.

        /// @brief Consume the `\` and the following escape character. Append the
        ///        resolved byte to `out`. Returns false if the escape is invalid
        ///        or the input ended mid-escape; in that case a diagnostic has
        ///        been reported at `escapeLoc`.
        bool readEscape(LexerState &s, std::string &out, SourceLocation escapeLoc)
        {
            advance(s); // consume `\`

            if (isAtEnd(s))
            {
                reportErrorAt(s, DiagCode::Lex_InvalidEscapeSequence, escapeLoc,
                              "unterminated escape sequence");
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
                reportErrorAt(s, DiagCode::Lex_InvalidEscapeSequence, escapeLoc,
                              std::string("unknown escape '\\") + next + "'");
                advance(s); // consume the offending character
                return false;
            }
        }

        // ─── Strings ──────────────────────────────────────────────────────────────
        //
        // One form:
        //
        //   "..."        normal string; escapes processed; no literal newline.
        //
        // The grammar has no raw-string form and no string interpolation.
        //
        // An invalid escape does not abort the string. The string lexer
        // continues reading to the closing `"`, tracking a `hadError` flag.
        // At the closing quote, it produces UNKNOWN if the flag is set,
        // STRING_LITERAL otherwise. The whole `"..."` is one token, so the
        // outer loop never sees a partial string.
        //
        // A literal newline is different: it means the string was not
        // terminated on its line. There is no way to consume the rest of
        // the string without swallowing the next line's content, so the
        // lexer bails out at the newline and produces UNKNOWN.

        /// @brief Lex a normal string. The cursor is on the opening `"`.
        void lexString(LexerState &s)
        {
            const SourceLocation startLoc = currentLocation(s);

            advance(s); // opening `"`

            std::string content;
            bool hadError = false;

            while (!isAtEnd(s))
            {
                const char c = currentChar(s);

                if (c == '"')
                {
                    advance(s); // closing `"`
                    s.tokens.push_back(makeTokenFromLexeme(
                        s,
                        hadError ? TokenType::UNKNOWN : TokenType::STRING_LITERAL,
                        content, startLoc));
                    return;
                }

                if (c == '\n')
                {
                    reportErrorAt(s, DiagCode::Lex_NewlineInString, startLoc,
                                  "a string literal cannot contain a newline");
                    s.tokens.push_back(makeTokenFromLexeme(
                        s, TokenType::UNKNOWN, content, startLoc));
                    return; // do NOT consume the newline; let the parser see it
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

        // ─── Character literals ───────────────────────────────────────────────────
        //
        // A char literal is exactly one character or one escape sequence between
        // single quotes. Escapes resolve to their actual byte, matching the string
        // lexer. `'\n'` is a CHAR_LITERAL whose value is the interned one-byte
        // string "\n" (the actual newline), not "\\n".
        //
        // The lexer consumes the entire `'...'` sequence as one token, even
        // when the content is malformed. This matters for a newline in the
        // middle: if the lexer bailed at the newline, the closing quote would
        // be seen as the start of a new char literal, producing a second
        // spurious diagnostic.

        void lexChar(LexerState &s)
        {
            const SourceLocation startLoc = currentLocation(s);

            advance(s); // opening `'`

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
                    // A newline inside a char literal is an error. The
                    // lexer keeps reading until the closing quote or
                    // end-of-input, so the whole `'...` is consumed as one
                    // malformed token.
                    reportErrorAt(s, DiagCode::Lex_UnterminatedCharLiteral, startLoc,
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
                    // More than one character of content. Report once
                    // and keep consuming until the closing quote.
                    if (!hadError)
                    {
                        reportErrorAt(s, DiagCode::Lex_UnterminatedCharLiteral, startLoc,
                                      "character literal contains more than one character");
                        hadError = true;
                    }
                    advance(s);
                }
            }

            if (!sawClosing)
            {
                reportErrorAt(s, DiagCode::Lex_UnterminatedCharLiteral, startLoc,
                              "unterminated character literal");
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
    // Every punctuation token's value is the punctuation's spelling, interned.
    // This is a deliberate uniformity choice: a token's value field is always
    // valid, so peekValue() never has to special-case "this token has no
    // spelling". The pool cost is one ID per distinct punctuation, not one per
    // occurrence.
    //
    // There are no compound operators. Each punctuation is one character.

    namespace
    {

        void lexPunctuation(LexerState &s)
        {
            const SourceLocation startLoc = currentLocation(s);
            const char c = currentChar(s);

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

            // ─── Unknown character ──────────────────────────────────────────────
            reportErrorAt(s, DiagCode::Lex_UnknownCharacter, startLoc,
                          std::string("unexpected character '") + c + "'");
            advance(s);
            s.tokens.push_back(makeTokenFromLexeme(
                s, TokenType::UNKNOWN, std::string_view(&c, 1), startLoc));
        }

    } // namespace

    // =============================================================================
    // 8. The dispatch and the public entry point
    // =============================================================================

    namespace
    {

        /// @brief Lex one token. The main loop calls this until EOF.
        void lexOne(LexerState &s)
        {
            skipWhitespace(s);

            if (isAtEnd(s))
            {
                // Intern "" once; the pool maps it to ID 0, which is what a
                // default-constructed InternedString holds, so this is free.
                s.tokens.push_back(makeToken(TokenType::EOF_TOKEN,
                                             InternedString{},
                                             currentLocation(s)));
                return;
            }

            const char c = currentChar(s);
            const char next = peekChar(s, 1);

            // ─── Comments ───────────────────────────────────────────────────────
            //
            // Order matters. `--` (line comment) and `/-` (block comment) start
            // with different characters, so their order in the chain does not
            // interact. There is no `//` form: `//` is not a comment in this
            // grammar.

            if (c == '-' && next == '-')
            {
                advance(s);
                advance(s); // consume `--`
                skipLineComment(s);
                return; // line comments are dropped
            }

            if (c == '/' && next == '-')
            {
                const SourceLocation startLoc = currentLocation(s);
                advance(s);
                advance(s); // consume `/-`
                bool terminated = false;
                readBlockComment(s, terminated);
                if (!terminated)
                {
                    reportErrorAt(s, DiagCode::Lex_UnterminatedBlockComment, startLoc,
                                  "unterminated block comment (expected -/)");
                }
                return; // block comments are dropped
            }

            // ─── Identifiers and keywords ───────────────────────────────────────
            if (isIdentifierStart(c))
            {
                lexIdentifier(s);
                return;
            }

            // ─── Numbers ────────────────────────────────────────────────────────
            // A `-` immediately followed by a digit starts a signed number.
            // A digit starts an unsigned number. A `.` is always DOT, even
            // when followed by a digit: the grammar's FLOAT_LIT requires a
            // digit before the `.`, so a leading `.` cannot begin a float.
            //
            // `--` and `/-` are matched above this block, so a `-` that
            // reaches here is not part of a comment.
            if (c == '-' && isDigit(next))
            {
                lexNumber(s);
                return;
            }
            if (isDigit(c))
            {
                lexNumber(s);
                return;
            }

            // ─── Strings ────────────────────────────────────────────────────────
            if (c == '"')
            {
                lexString(s);
                return;
            }

            // ─── Char literal ───────────────────────────────────────────────────
            if (c == '\'')
            {
                lexChar(s);
                return;
            }

            // ─── Punctuation ────────────────────────────────────────────────────
            lexPunctuation(s);
        }

    } // namespace

    std::vector<Token> tokenize(std::string_view source,
                                StringPool &pool,
                                lucid::diag::DiagnosticEngine &diagnostics)
    {
        LexerState s(source, pool, diagnostics);

        while (true)
        {
            lexOne(s);
            if (!s.tokens.empty() &&
                s.tokens.back().type == TokenType::EOF_TOKEN)
            {
                break;
            }
        }

        return std::move(s.tokens);
    }

} // namespace lucid::lexer