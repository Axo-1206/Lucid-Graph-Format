```
lucid_format/                          ← the new repo
│
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .gitignore
│
├── docs/
│   ├── grammar/LUCID_GRAMMAR.md       ← copy of Grammar.md
│   ├── ARCHITECTURE.md
│   ├── FORMATTER.md                   ← new: how the formatter works
│   └── BUILD.md
│
├── core/                              ← format-independent infrastructure
│   ├── include/core/
│   │   ├── SourceLocation.hpp         ← kept
│   │   ├── Tokens.hpp                 ← rewritten for the new grammar
│   │   ├── ASTStrings.hpp             ← rewritten
│   │   ├── Primitives.hpp             ← kept (PrimitiveKind)
│   │   │
│   │   ├── ast/
│   │   │   ├── BaseAST.hpp            ← adapted
│   │   │   ├── DeclAST.hpp            ← rewritten (Import/Enum/Resource/Node/Composite)
│   │   │   ├── ValueAST.hpp           ← new: the four value forms
│   │   │   ├── TypeAST.hpp            ← rewritten (type_id only)
│   │   │   └── AttributeAST.hpp       ← new: @export
│   │   │
│   │   ├── memory/
│   │   │   ├── ASTArena.hpp           ← kept
│   │   │   ├── ArenaSpan.hpp          ← kept
│   │   │   ├── InternedString.hpp     ← kept
│   │   │   └── StringPool.hpp         ← kept
│   │   │
│   │   ├── diagnostics/
│   │   │   ├── DiagCode.hpp           ← rewritten for the new grammar
│   │   │   ├── Diagnostic.hpp         ← kept
│   │   │   └── StackTrace.hpp         ← kept
│   │   │
│   │   └── trivia/
│   │       ├── Trivia.hpp             ← new: comment + whitespace
│   │       └── TriviaBuffer.hpp       ← new: trivia attached to tokens
│   │
│   └── src/core/
│       ├── Tokens.cpp                 ← rewritten
│       ├── ASTStrings.cpp             ← rewritten
│       ├── memory/
│       │   └── StringPool.cpp         ← kept
│       ├── diagnostics/
│       │   ├── Diagnostic.cpp         ← kept
│       │   └── StackTrace.cpp         ← kept
│       └── trivia/
│           └── TriviaBuffer.cpp       ← new
│
├── parser/                            ← lexer + parser
│   ├── include/parser/
│   │   ├── Parser.hpp                 ← rewritten for the new grammar
│   │   └── Lexer.hpp                  ← rewritten for the new grammar
│   │
│   └── src/parser/
│       ├── Parser.cpp
│       ├── lexer/
│       │   ├── Lexer.cpp
│       │   └── TriviaScanner.hpp/.cpp ← new: capture comments + whitespace
│       ├── context/
│       │   ├── ParserContext.cpp      ← adapted
│       │   └── TokenStream.cpp   ← adapted
│       ├── rules/
│       │   ├── ParseDecl.cpp          ← import/enum/resource/node/composite
│       │   ├── ParseType.cpp          ← type_id
│       │   ├── ParseValue.cpp         ← value (4 forms)
│       │   ├── ParseNode.cpp          ← node_expr + trigger_list
│       │   └── ParseComposite.cpp     ← composite body
│       └── support/
│           ├── ErrorRecovery.hpp      ← new
│           ├── Helpers.cpp
│           └── GrammarPositions.hpp/.cpp
│
├── formatter/                         ← NEW: the formatter itself
│   ├── include/formatter/
│   │   ├── Formatter.hpp              ← public entry point
│   │   ├── FormatOptions.hpp          ← indent width, etc.
│   │   ├── CommentAttacher.hpp        ← attach trivia to AST nodes
│   │   └── FormatterError.hpp
│   │
│   └── src/formatter/
│       ├── Formatter.cpp
│       ├── FormatOptions.cpp
│       ├── CommentAttacher.cpp
│       └── layout/
│           ├── IndentWriter.hpp/.cpp  ← writes with indentation
│           ├── LineBreaker.hpp/.cpp   ← decides where to break
│           └── LayoutBuilder.hpp/.cpp ← builds the output
│
├── sema/                              ← DEFERRED — placeholder only
│   └── README.md                      ← "Semantic analysis is deferred."
│
├── tools/                             ← optional CLI, later
│   └── README.md
│
├── src/
│   └── main.cpp                       ← lucid-fmt entry point (later)
│
└── tests/
    ├── CMakeLists.txt
    ├── core/
    │   ├── test_string_pool.cpp
    │   ├── test_ast_arena.cpp
    │   ├── test_source_location.cpp
    │   └── test_tokens.cpp
    ├── parser/
    │   ├── test_lexer.cpp
    │   ├── test_parser_decl.cpp
    │   ├── test_parser_value.cpp
    │   ├── test_parser_composite.cpp
    │   └── test_token_stream.cpp
    ├── formatter/
    │   ├── test_formatter_basic.cpp
    │   ├── test_formatter_comments.cpp
    │   └── test_formatter_idempotent.cpp
    └── fixtures/
        ├── input/
        │   ├── empty.lucid
        │   ├── imports.lucid
        │   ├── resources.lucid
        │   ├── nodes.lucid
        │   ├── composites.lucid
        │   └── comments.lucid
        └── expected/
            ├── empty.lucid
            ├── imports.lucid
            ├── resources.lucid
            ├── nodes.lucid
            ├── composites.lucid
            └── comments.lucid
```