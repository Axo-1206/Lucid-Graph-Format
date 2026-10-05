# Lucid Format

A standalone formatter library for the **Lucid Graph Format** — the
declarative format described in `docs/grammar/LUCID_GRAMMAR.md`.

## What this is

A pure-C++ library that reads `.lucid` source text and produces
pretty-printed text. It is the front half of a larger toolchain:

- **Lexer** — text → tokens.
- **Parser** — tokens → AST.
- **Formatter** — AST → formatted text.

Semantic analysis is deferred; a placeholder lives in `sema/`.

## What this is not

- Not the compiler. No bytecode, no interpreter, no runtime.
- Not the engine. No rendering, physics, audio, or platform code.
- Not a semantic analyzer. The parser is purely syntactic.

## Layout

| Directory    | Contents                                                                                   |
| ------------ | ------------------------------------------------------------------------------------------ |
| `core/`      | Format-independent infrastructure: strings, arena, diagnostics, tokens, AST bases, trivia. |
| `parser/`    | Lexer and parser. Text in, `ModuleAST` out.                                                |
| `formatter/` | Formatter. `ModuleAST` in, formatted text out.                                             |
| `sema/`      | Deferred. Placeholder only.                                                                |
| `tools/`     | Deferred. Placeholder only.                                                                |
| `tests/`     | Per-layer tests plus fixture files.                                                        |
| `docs/`      | Grammar, architecture, formatter design.                                                   |

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure