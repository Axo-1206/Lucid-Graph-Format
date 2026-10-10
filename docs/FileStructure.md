```
Lucid-Graph-Format/
│
├── CMAKELists.txt
├── README.md
├── LICENSE
├── .gitignore
│
├── docs/
│   ├── Architecture.md
│   ├── FileStructure.md               ← this file
│   ├── Grammar.md
│   └── notes/
│       └── RegistryContractNote.md
│
├── core/                              ← format-independent infrastructure
│   ├── include/core/
│   │   ├── SourceLocation.hpp         ← source location tracking for the formatter's pipeline
│   │   ├── Tokens.hpp
│   │   │
│   │   ├── ast/
│   │   │   ├── AttributeAST.hpp       ← AST node for one attribute prefix: `@name`
│   │   │   ├── BaseAST.hpp
│   │   │   ├── DeclAST.hpp            ← AST nodes for the four top-level declarations
│   │   │   ├── ModuleAST.hpp          ← root of a parsed file
│   │   │   ├── TypeAST.hpp            ← AST node for a type reference: `type_id`
│   │   │   └── ValueAST.hpp           ← AST nodes for the four value forms
│   │   │
│   │   ├── diagnostics/
│   │   │   ├── DiagCode.hpp           ← diagnostic code space: severity, category, and codes
│   │   │   └── Diagnostic.hpp         ← diagnostic engine: collection, query, and formatting
│   │   │
│   │   ├── memory/
│   │   │   ├── ASTArena.hpp           ← bump allocator for AST nodes
│   │   │   ├── ArenaSpan.hpp          ← non-owning, const view of a contiguous run of elements
│   │   │   ├── InternedString.hpp     ← 32-bit handle to a string owned by a StringPool
│   │   │   └── StringPool.hpp         ← owns all interned string data for a compilation session
│   │   │
│   │   └── trivia/
│   │       ├── Trivia.hpp             ← one piece of trivia: a comment in the source
│   │       └── TriviaBuffer.hpp       ← sorted list of comments from one source file
│   │
│   └── src/core/
│       ├── Tokens.cpp                 ← name functions for the token types
│       ├── diagnostics/
│       │   └── Diagnostic.cpp         ← implementation of DiagnosticEngine
│       └── memory/
│           └── StringPool.cpp         ← implementation of StringPool
│
├── parser/                            ← lexer + parser
│   ├── include/parser/
│   │   ├── Parser.hpp                 ← the Lucid parser: one file in, one ModuleAST out
│   │   ├── context/
│   │   │   ├── ParserContext.hpp      ← per-session parser state
│   │   │   └── TokenStream.hpp        ← forward-only cursor over one file's token vector
│   │   ├── dump/
│   │   │   ├── JSONDumper.hpp         ← AST → JSON for the Lucid Graph Format
│   │   │   └── JSONWriter.hpp         ← small streaming JSON writer
│   │   ├── lexer/
│   │   │   └── Lexer.hpp              ← converts one source file's text into a flat token stream
│   │   └── support/
│   │       ├── ErrorRecovery.hpp      ← synchronization utilities for the Lucid parser
│   │       └── GrammarPositions.hpp   ← parser's start-set predicates
│   │
│   └── src/parser/
│       ├── Parser.cpp                 ← parser's entry point and top-level dispatcher
│       ├── context/
│       │   └── TokenStream.cpp        ← implementation of TokenStream
│       ├── dump/
│       │   └── JSONDumper.cpp         ← implementation of the AST → JSON dumper
│       ├── lexer/
│       │   └── Lexer.cpp              ← implementation of the Lucid lexer
│       └── rules/
│           ├── ParseDecl.cpp          ← declaration parsers, attribute parsers
│           ├── ParseDeclInternal.hpp  ← internal declarations shared between rules/ files
│           ├── ParseNode.cpp          ← parseNodeExpr, parseArgList, parseTriggerList
│           ├── ParseType.cpp          ← implementation of parseTypeId
│           └── ParseValue.cpp         ← implementation of parseValue and parseLiteral
│
├── formatter/                         ← the formatter itself
│   ├── include/formatter/
│   │   ├── FormatOptions.hpp          ← tunables for the formatter
│   │   ├── Formatter.hpp              ← the formatter's public API
│   │   └── IndentWriter.hpp           ← text emitter that tracks indent level and column
│   │
│   └── src/formatter/
│       ├── Formatter.cpp              ← implementation of the formatter's entry points
│       ├── LayoutBuilder.hpp          ← AST → text walker
│       └── LayoutBuilder.cpp          ← implementation of the AST → text walker
│
├── sema/                              ← semantic analysis
│   ├── include/sema/
│   │   ├── ConstantValueMap.hpp       ← result of constant folding
│   │   ├── Graph.hpp                  ← output of Sema: a flat, resolved graph
│   │   ├── Literal.hpp                ← a compile-time-known value
│   │   ├── ModuleTable.hpp            ← map from a module's name to its symbol table
│   │   ├── NodeKind.hpp               ← the three kinds of node
│   │   ├── Primitives.hpp             ← primitive types and their aliases
│   │   ├── Registry.hpp               ← engine's declarations of node types, types, and phases
│   │   ├── ResolutionMap.hpp          ← result of Pass 2: what every reference resolved to
│   │   ├── Sema.hpp                   ← public entry points of the semantic analyzer
│   │   ├── SymbolTable.hpp            ← per-module symbol table
│   │   ├── TypeId.hpp                 ← reference to a type, in the registry's vocabulary
│   │   ├── TypeMap.hpp                ← result of Pass 3: what type every expression has
│   │   └── dump/
│   │       └── GraphDumper.hpp        ← serialize a Graph to JSON
│   │
│   └── src/sema/
│       ├── AttributeChecker.hpp       ← Sema's attribute validation pass
│       ├── AttributeChecker.cpp       ← implementation of attribute validation
│       ├── DeadCodeChecker.hpp        ← Pass 5: report unused value nodes
│       ├── DeadCodeChecker.cpp        ← implementation of Pass 5: dead-code detection
│       ├── DeclHelpers.hpp            ← small helpers shared between Sema passes
│       ├── GraphBuilder.hpp           ← Pass 4: build the Graph from resolved modules
│       ├── GraphBuilder.cpp           ← implementation of Pass 4: graph construction
│       ├── ImportResolver.hpp         ← loads a module's transitively-imported modules
│       ├── ImportResolver.cpp         ← implementation of import loading
│       ├── Registry.cpp               ← implementation of computeRegistryFingerprint
│       ├── Resolver.hpp               ← Pass 2: resolve names against the symbol table
│       ├── Resolver.cpp               ← implementation of Pass 2: name resolution
│       ├── Sema.cpp                   ← implementation of Sema's public entry points
│       ├── SymbolCollector.hpp        ← Pass 1: build the per-module symbol table
│       ├── SymbolCollector.cpp        ← implementation of Pass 1's symbol collector
│       ├── SymbolKindOf.hpp
│       ├── SymbolTable.cpp            ← implementation of SymbolTable
│       ├── TypeChecker.hpp            ← Pass 3: assign types, check consistency, enforce rules
│       ├── TypeChecker.cpp            ← implementation of Pass 3: type checking and trigger rules
│       └── dump/
│           └── GraphDumper.cpp        ← implementation of the Graph JSON dumper
│
├── serialization/                     ← binary .lucgraph format
│   ├── include/serialization/
│   │   ├── BinaryFormat.hpp           ← byte-level constants of the .lucgraph format
│   │   ├── ByteSpan.hpp               ← read-only view over a contiguous byte buffer
│   │   ├── Deserializer.hpp           ← read a .lucgraph byte buffer into a Graph
│   │   └── Serializer.hpp             ← write a Graph to a .lucgraph byte vector
│   │
│   └── src/serialization/
│       ├── Deserializer.cpp           ← implementation of deserialize()
│       ├── Reader.hpp                 ← low-level byte reader used by the deserializer
│       ├── Serializer.cpp             ← implementation of serialize(const Graph&)
│       └── Writer.hpp                 ← low-level byte writer used by the serializer
│
├── cli/                               ← command-line interface
│   ├── include/cli/
│   │   ├── CLIOptions.hpp             ← parsed command-line options for the `lucid` tool
│   │   ├── Registry.hpp               ← default registry shared by the CLI commands
│   │   ├── Version.hpp                ← CLI tools' version string
│   │   └── commands/
│   │       ├── Check.hpp              ← `lucid-check` command's entry point
│   │       ├── Compile.hpp            ← `lucid-compile` command's entry point
│   │       └── Format.hpp             ← `lucid-fmt` command's entry point
│   │
│   └── src/cli/
│       ├── CLIOptions.cpp             ← implementation of CLIOptions::parse
│       ├── Main.cpp                   ← `lucid` entry point and subcommand dispatcher
│       ├── Registry.cpp               ← default registry's definition
│       └── commands/
│           ├── Check.cpp              ← implementation of the `lucid check` subcommand
│           ├── Compile.cpp            ← implementation of the `lucid compile` subcommand
│           └── Format.cpp             ← implementation of the `lucid format` subcommand
│
└── tests/
    ├── CMakeLists.txt
    ├── core/
    │   ├── test_ast_arena.cpp          ← tests for ASTArena and ArenaSpan
    │   ├── test_attribute_ast.cpp      ← tests for AttributeAST
    │   ├── test_base_ast.cpp           ← tests for BaseAST, ASTKind, isa/as, and UnknownAST
    │   ├── test_core_smoke.cpp         ← phase 0 smoke test for lucid_core
    │   ├── test_decl_ast.cpp           ← tests for DeclAST.hpp family and eight concrete nodes
    │   ├── test_module_ast.cpp         ← tests for ModuleAST
    │   ├── test_source_location.cpp    ← tests for SourceLocation: packing, accessors, limits
    │   ├── test_string_pool.cpp        ← tests for StringPool and InternedString
    │   ├── test_tokens.cpp             ← tests for the token vocabulary
    │   ├── test_trivia.cpp             ← tests for Trivia and TriviaBuffer
    │   ├── test_type_ast.cpp           ← tests for TypeIdAST
    │   └── test_value_ast.cpp          ← tests for ValueAST.hpp: NodeExprAST and four value nodes
    ├── parser/
    │   ├── test_error_recovery.cpp     ← tests for parser/support/ErrorRecovery.hpp
    │   ├── test_fixtures.cpp           ← fixture-driven tests for the parser and the dumper
    │   ├── test_grammar_positions.cpp  ← tests for parser/support/GrammarPositions.hpp
    │   ├── test_json_dumper.cpp        ← tests for the AST → JSON dumper
    │   ├── test_json_writer.cpp        ← tests for JSONWriter
    │   ├── test_lexer.cpp              ← tests for the Lucid lexer
    │   ├── test_parse_decl.cpp         ← tests for the declaration parsers in ParseDecl.cpp
    │   ├── test_parse_file.cpp         ← end-to-end tests for parseFile
    │   ├── test_parse_node.cpp         ← tests for parseNodeExpr, parseArgList, parseTriggerList
    │   ├── test_parse_type.cpp         ← tests for parseTypeId
    │   ├── test_parse_value.cpp        ← tests for parseValue and parseLiteral
    │   ├── test_parser_context.cpp     ← tests for ParserContext and ScopedDiagnosticFile
    │   ├── test_parser_smoke.cpp       ← phase 0 smoke test for lucid_parser
    │   ├── test_token_stream.cpp       ← tests for TokenStream
    │   └── test_tokenize_with_trivia.cpp ← tests for the lexer's trivia collection
    ├── formatter/
    │   ├── test_format_options.cpp     ← tests for FormatOptions
    │   ├── test_formatter_api.cpp      ← tests for the formatter's public API
    │   ├── test_formatter_fixtures.cpp ← fixture-driven round-trip tests for the formatter
    │   ├── test_formatter_smoke.cpp    ← phase 0 smoke test for lucid_formatter
    │   ├── test_indent_writer.cpp      ← tests for IndentWriter
    │   └── test_layout_builder.cpp     ← tests for LayoutBuilder
    ├── sema/
    │   ├── test_constant_value_map.cpp ← tests for ConstantValueMap
    │   ├── test_dead_code.cpp          ← tests for Pass 5: dead-code detection
    │   ├── test_graph.cpp              ← tests for the Graph data types
    │   ├── test_graph_builder.cpp      ← tests for Pass 4: graph construction
    │   ├── test_import_resolver.cpp    ← tests for the import resolver
    │   ├── test_literal.cpp            ← tests for Literal
    │   ├── test_multi_module.cpp       ← end-to-end tests for multi-module compiles
    │   ├── test_node_kind.cpp          ← tests for NodeKind
    │   ├── test_pipeline_smoke.cpp     ← single end-to-end test for the whole compile pipeline
    │   ├── test_primitives.cpp         ← tests for the primitive alias table
    │   ├── test_registry.cpp           ← tests for the Registry data types
    │   ├── test_resolution_map.cpp     ← tests for ResolutionMap
    │   ├── test_resolver.cpp           ← tests for Pass 2: name resolution
    │   ├── test_sema_api.cpp           ← smoke tests for Sema's public API
    │   ├── test_sema_fixtures.cpp      ← fixture-driven end-to-end tests for Sema
    │   ├── test_sema_smoke.cpp         ← smoke test that Sema headers include cleanly
    │   ├── test_symbol_collector.cpp   ← tests for Pass 1's symbol collector
    │   ├── test_symbol_table.cpp       ← tests for SymbolTable
    │   ├── test_type_checker.cpp       ← tests for Pass 3: type checking and trigger rules
    │   ├── test_type_id.cpp            ← tests for TypeId
    │   └── test_type_map.cpp           ← tests for TypeMap
    ├── serialization/
    │   ├── test_deserialize_errors.cpp ← error-path tests for deserialize()
    │   ├── test_roundtrip.cpp          ← round-trip: deserialize(serialize(g)) == g
    │   └── test_serializer.cpp         ← unit tests for serialize()
    ├── cli/
    │   ├── test_check_command.cpp      ← tests for the lucid-check command implementation
    │   ├── test_cli_options.cpp        ← unit tests for CLIOptions::parse with subcommands
    │   ├── test_compile_command.cpp    ← unit tests for the `lucid-compile` command
    │   └── test_format_command.cpp     ← tests for the lucid-fmt command implementation
    ├── tools/
    │   ├── fixture_regen.cpp           ← regenerates expected fixture output
    │   └── SemaFixtureCommon.hpp
    ├── integration/
    │   ├── run_lucid_tests.py          ← integration test runner script
    │   └── fixtures/
    │       ├── bad_syntax.lucid
    │       └── simple.lucid
    └── fixtures/
        ├── parser/
        │   ├── good/                   ← valid .lucid + expected .json AST dumps
        │   │   ├── empty.lucid / empty.json
        │   │   ├── enum-single.lucid / enum-single.json
        │   │   ├── enum-multi.lucid / enum-multi.json
        │   │   ├── enum-trailing-comma.lucid / enum-trailing-comma.json
        │   │   ├── import-simple.lucid / import-simple.json
        │   │   ├── import-aliased.lucid / import-aliased.json
        │   │   ├── import-dotted.lucid / import-dotted.json
        │   │   ├── node-no-args.lucid / node-no-args.json
        │   │   ├── node-args.lucid / node-args.json
        │   │   ├── node-trigger.lucid / node-trigger.json
        │   │   ├── node-multiple-triggers.lucid / node-multiple-triggers.json
        │   │   ├── node-inline-arg.lucid / node-inline-arg.json
        │   │   ├── node-trailing-comma.lucid / node-trailing-comma.json
        │   │   ├── resource-empty.lucid / resource-empty.json
        │   │   ├── resource-fields.lucid / resource-fields.json
        │   │   ├── resource-qualified-type.lucid / resource-qualified-type.json
        │   │   ├── attribute-export-resource.lucid / attribute-export-resource.json
        │   │   └── realistic.lucid / realistic.json
        │   └── bad/                    ← invalid .lucid + expected error output
        │       ├── attribute-missing-name.lucid / attribute-missing-name.expected
        │       ├── enum-missing-body.lucid / enum-missing-body.expected
        │       ├── enum-missing-name.lucid / enum-missing-name.expected
        │       ├── import-missing-path.lucid / import-missing-path.expected
        │       ├── node-field-access-missing-field.lucid / ...expected
        │       ├── node-missing-arglist.lucid / node-missing-arglist.expected
        │       ├── node-missing-equals.lucid / node-missing-equals.expected
        │       ├── node-missing-name.lucid / node-missing-name.expected
        │       ├── node-missing-trigger.lucid / node-missing-trigger.expected
        │       ├── node-type-third-segment.lucid / node-type-third-segment.expected
        │       ├── resource-field-missing-default.lucid / ...expected
        │       ├── resource-missing-colon.lucid / resource-missing-colon.expected
        │       ├── resource-missing-name.lucid / resource-missing-name.expected
        │       ├── top-level-stray-brace.lucid / top-level-stray-brace.expected
        │       └── top-level-unknown-token.lucid / top-level-unknown-token.expected
        ├── formatter/
        │   ├── canonical/              ← messy input → canonical expected output
        │   │   ├── attributes.lucid / attributes.expected
        │   │   ├── comments-block.lucid / comments-block.expected
        │   │   ├── comments-both.lucid / comments-both.expected
        │   │   ├── comments-leading.lucid / comments-leading.expected
        │   │   ├── comments-trailing.lucid / comments-trailing.expected
        │   │   ├── messy-enum.lucid / messy-enum.expected
        │   │   ├── messy-import.lucid / messy-import.expected
        │   │   ├── messy-node.lucid / messy-node.expected
        │   │   ├── messy-resource.lucid / messy-resource.expected
        │   │   └── string-escapes.lucid / string-escapes.expected
        │   └── idempotent/             ← already-formatted input (round-trip stable)
        │       ├── comments-block.lucid
        │       ├── comments.lucid
        │       ├── empty.lucid
        │       ├── enum.lucid
        │       ├── import.lucid
        │       ├── node.lucid
        │       ├── realistic.lucid
        │       └── resource.lucid
        └── sema/
            ├── action-with-trigger/ (input.lucid / expected.json)
            ├── comments/ (input.lucid / expected.json)
            ├── empty/ (input.lucid / expected.json)
            ├── resource/ (input.lucid / expected.json)
            ├── simple-node/ (input.lucid / expected.json)
            ├── two-nodes/ (input.lucid / expected.json)
            └── with-import/ (input.lucid / keys.lucid / expected.json)
```
