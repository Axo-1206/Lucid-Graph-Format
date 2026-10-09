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
│   │   ├── SourceLocation.hpp
│   │   ├── Tokens.hpp
│   │   │
│   │   ├── ast/
│   │   │   ├── AttributeAST.hpp
│   │   │   ├── BaseAST.hpp
│   │   │   ├── DeclAST.hpp
│   │   │   ├── ModuleAST.hpp
│   │   │   ├── TypeAST.hpp
│   │   │   └── ValueAST.hpp
│   │   │
│   │   ├── diagnostics/
│   │   │   ├── DiagCode.hpp
│   │   │   └── Diagnostic.hpp
│   │   │
│   │   ├── memory/
│   │   │   ├── ASTArena.hpp
│   │   │   ├── ArenaSpan.hpp
│   │   │   ├── InternedString.hpp
│   │   │   └── StringPool.hpp
│   │   │
│   │   └── trivia/
│   │       ├── Trivia.hpp
│   │       └── TriviaBuffer.hpp
│   │
│   └── src/core/
│       ├── Tokens.cpp
│       ├── diagnostics/
│       │   └── Diagnostic.cpp
│       └── memory/
│           └── StringPool.cpp
│
├── parser/                            ← lexer + parser
│   ├── include/parser/
│   │   ├── Parser.hpp
│   │   ├── context/
│   │   │   ├── ParserContext.hpp
│   │   │   └── TokenStream.hpp
│   │   ├── dump/
│   │   │   ├── JSONDumper.hpp
│   │   │   └── JSONWriter.hpp
│   │   ├── lexer/
│   │   │   └── Lexer.hpp
│   │   └── support/
│   │       ├── ErrorRecovery.hpp
│   │       └── GrammarPositions.hpp
│   │
│   └── src/parser/
│       ├── Parser.cpp
│       ├── context/
│       │   └── TokenStream.cpp
│       ├── dump/
│       │   └── JSONDumper.cpp
│       ├── lexer/
│       │   └── Lexer.cpp
│       └── rules/
│           ├── ParseDecl.cpp          ← import/enum/resource/node
│           ├── ParseDeclInternal.hpp
│           ├── ParseNode.cpp          ← node_expr + trigger_list
│           ├── ParseType.cpp          ← type_id
│           └── ParseValue.cpp         ← value (4 forms)
│
├── formatter/                         ← the formatter itself
│   ├── include/formatter/
│   │   ├── FormatOptions.hpp          ← indent width, etc.
│   │   ├── Formatter.hpp              ← public entry point
│   │   └── IndentWriter.hpp           ← writes with indentation
│   │
│   └── src/formatter/
│       ├── Formatter.cpp
│       ├── LayoutBuilder.hpp          ← builds the output
│       └── LayoutBuilder.cpp
│
├── sema/                              ← semantic analysis
│   ├── include/sema/
│   │   ├── Graph.hpp
│   │   ├── Literal.hpp
│   │   ├── NodeKind.hpp
│   │   ├── Primitives.hpp
│   │   ├── Registry.hpp
│   │   ├── ResolutionMap.hpp
│   │   ├── Sema.hpp
│   │   ├── SymbolTable.hpp
│   │   ├── TypeId.hpp
│   │   └── TypeMap.hpp
│   │
│   └── src/sema/
│       ├── Registry.cpp
│       ├── Resolver.hpp
│       ├── Resolver.cpp
│       ├── Sema.cpp
│       ├── SymbolCollector.hpp
│       ├── SymbolCollector.cpp
│       ├── SymbolTable.cpp
│       ├── TypeChecker.hpp
│       └── TypeChecker.cpp
│
├── cli/                               ← command-line interface
│   ├── include/cli/
│   │   ├── CLIOptions.hpp
│   │   └── commands/
│   │       └── Format.hpp
│   │
│   └── src/cli/
│       ├── CLIOptions.cpp
│       ├── Main.cpp
│       └── commands/
│           └── Format.cpp
│
├── src/
│   └── main.cpp                       ← lucid-fmt entry point
│
└── tests/
    ├── CMakeLists.txt
    ├── core/
    │   ├── test_ast_arena.cpp
    │   ├── test_attribute_ast.cpp
    │   ├── test_base_ast.cpp
    │   ├── test_core_smoke.cpp
    │   ├── test_decl_ast.cpp
    │   ├── test_module_ast.cpp
    │   ├── test_source_location.cpp
    │   ├── test_string_pool.cpp
    │   ├── test_tokens.cpp
    │   ├── test_trivia.cpp
    │   ├── test_type_ast.cpp
    │   └── test_value_ast.cpp
    ├── parser/
    │   ├── test_error_recovery.cpp
    │   ├── test_fixtures.cpp
    │   ├── test_grammar_positions.cpp
    │   ├── test_json_dumper.cpp
    │   ├── test_json_writer.cpp
    │   ├── test_lexer.cpp
    │   ├── test_parse_decl.cpp
    │   ├── test_parse_file.cpp
    │   ├── test_parse_node.cpp
    │   ├── test_parse_type.cpp
    │   ├── test_parse_value.cpp
    │   ├── test_parser_context.cpp
    │   ├── test_parser_smoke.cpp
    │   ├── test_token_stream.cpp
    │   └── test_tokenize_with_trivia.cpp
    ├── formatter/
    │   ├── test_format_options.cpp
    │   ├── test_formatter_api.cpp
    │   ├── test_formatter_fixtures.cpp
    │   ├── test_formatter_smoke.cpp
    │   ├── test_indent_writer.cpp
    │   └── test_layout_builder.cpp
    ├── sema/
    │   ├── test_graph.cpp
    │   ├── test_literal.cpp
    │   ├── test_node_kind.cpp
    │   ├── test_primitives.cpp
    │   ├── test_registry.cpp
    │   ├── test_resolution_map.cpp
    │   ├── test_resolver.cpp
    │   ├── test_sema_api.cpp
    │   ├── test_sema_smoke.cpp
    │   ├── test_symbol_collector.cpp
    │   ├── test_symbol_table.cpp
    │   ├── test_type_checker.cpp
    │   ├── test_type_id.cpp
    │   └── test_type_map.cpp
    ├── cli/
    │   ├── test_cli_options.cpp
    │   └── test_format_command.cpp
    ├── tools/
    │   └── fixture_regen.cpp          ← regenerates expected fixture output
    └── fixtures/
        ├── parser/
        │   ├── good/                  ← valid .lucid + expected .json AST dumps
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
        │   └── bad/                   ← invalid .lucid + expected error output
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
        └── formatter/
            ├── canonical/             ← messy input → canonical expected output
            │   ├── attributes.lucid / attributes.expected
            │   ├── comments-block.lucid / comments-block.expected
            │   ├── comments-both.lucid / comments-both.expected
            │   ├── comments-leading.lucid / comments-leading.expected
            │   ├── comments-trailing.lucid / comments-trailing.expected
            │   ├── messy-enum.lucid / messy-enum.expected
            │   ├── messy-import.lucid / messy-import.expected
            │   ├── messy-node.lucid / messy-node.expected
            │   ├── messy-resource.lucid / messy-resource.expected
            │   └── string-escapes.lucid / string-escapes.expected
            └── idempotent/            ← already-formatted input (round-trip stable)
                ├── comments-block.lucid
                ├── comments.lucid
                ├── empty.lucid
                ├── enum.lucid
                ├── import.lucid
                ├── node.lucid
                ├── realistic.lucid
                └── resource.lucid
```
