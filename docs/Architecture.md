# Lucid Format Library — Architecture

The format library, `lucid_format`, is the tool that reads, validates, and writes Lucid graph files. This document describes its architecture: the layers, the components, and how they fit together.

---

## 1. Purpose and scope

`lucid_format` is a **standalone library**. It has no dependency on the engine, the renderer, the physics backend, or any platform API. It is pure C++.

It performs four jobs:

1. **Lex and parse** a `.lucid` file into an abstract syntax tree (AST).
2. **Format** an AST back into canonical `.lucid` text.
3. **Validate** an AST against a host-provided registry (this is Sema).
4. **Serialize and deserialize** a validated graph into and out of a `.lucgraph` binary.

It exposes a small API. Everything else is internal.

The library is designed to be usable by:

- The **engine**, at runtime, for loading graphs.
- The **editor**, for validation and preview.
- The **build tool**, for producing `.lucgraph` files.
- **Tools** — linters, formatters, visualizers, CI validators.

Each of these uses a subset of the library. The editor uses everything. A formatter tool uses only the parser and formatter. A shipping game uses only the deserializer.

---

## 2. The layers

The library is organized into five layers, each building on the one below it.

```
┌─────────────────────────────────────────────────────────┐
│  sema        ── AST + Registry → Graph                  │
├─────────────────────────────────────────────────────────┤
│  formatter   ── AST → canonical text                    │
├─────────────────────────────────────────────────────────┤
│  parser      ── tokens → AST                            │
├─────────────────────────────────────────────────────────┤
│  core        ── tokens, AST types, diagnostics, memory  │
└─────────────────────────────────────────────────────────┘
```

Plus two cross-cutting pieces that sit alongside the layers:

- **serialization** — `Graph` ↔ binary bytes.
- **the `Graph` structure** — the shared output type of Sema, the shared input type of the engine.

Dependencies flow upward only. `core` depends on nothing. `parser` depends on `core`. `formatter` depends on `core` and `parser`. `sema` depends on `core` and `parser`. `serialization` depends on `core` and the `Graph` structure. Nothing depends on the engine.

---

## 3. Layer 1 — `core`

The `core` layer contains the pieces that every other layer needs: tokens, AST node types, diagnostics, memory management, source locations, and trivia.

### 3.1 Responsibilities

- Define token kinds and the `Token` structure.
- Define all AST node types.
- Provide source location tracking.
- Provide an arena allocator for AST nodes.
- Provide a string pool for interned strings.
- Define the diagnostic system.
- Capture trivia (comments and whitespace) attached to tokens.

### 3.2 Key components

**`SourceLocation.hpp`** — a position in a source file: byte offset, line, column. Attached to every token and every AST node.

**`TokenKind.hpp`** — the enum of every token kind the lexer can produce. Predicates (`is_keyword`, `is_literal`, `is_punct`) live here so that both the parser and the formatter share them.

**`Tokens.hpp`** — the `Token` structure: kind, lexeme, source location, and attached trivia. A token is a value type; tokens are cheap to copy.

**`ASTStrings.hpp`** — a small set of `constexpr` string constants used in the AST (declaration names, etc.).

**`Primitives.hpp`** — the `PrimitiveKind` enum (`Bool`, `Char`, `String`, `Int8`, ..., `Float64`).

**`ast/BaseAST.hpp`** — the base class for every AST node. Provides the node kind tag, source location, and the arena-friendly layout.

**`ast/DeclAST.hpp`** — the five declaration kinds: `ImportDecl`, `EnumDecl`, `ResourceDecl`, `NodeDecl`, `CompositeDecl`.

**`ast/ValueAST.hpp`** — the four value forms: `LiteralValue`, `IdentifierValue`, `FieldAccessValue`, `InlineNodeValue`.

**`ast/TypeAST.hpp`** — `TypeIdAST`, representing a qualified type name (`Key`, `core.Key`).

**`ast/AttributeAST.hpp`** — `AttributeAST`, representing one `@name` prefix.

**`memory/ASTArena.hpp`** — a bump-pointer arena that owns all AST nodes for a parse. Frees everything at once.

**`memory/ArenaSpan.hpp`** — a lightweight span into the arena, used for lists of nodes.

**`memory/InternedString.hpp`** — a handle to a string in the string pool.

**`memory/StringPool.hpp`** — deduplicates strings, provides stable IDs.

**`diagnostics/DiagCode.hpp`** — the enum of every diagnostic the library can emit.

**`diagnostics/Diagnostic.hpp`** — a diagnostic: code, severity, message, source location, related locations.

**`trivia/Trivia.hpp`** — one piece of trivia: a comment or a run of whitespace.

**`trivia/TriviaBuffer.hpp`** — a buffer of trivia attached to a token. Used by the formatter to preserve comments.

### 3.3 Design notes

The `core` layer has no knowledge of grammar structure beyond the AST node types. It does not know how to build an AST (that's the parser) or what to do with one (that's `sema` or the formatter). It defines the *vocabulary*, not the *sentences*.

The AST arena is the key memory decision. A parse allocates all AST nodes from a single arena. When the parse is finished, the arena is freed in one operation. There is no per-node deletion, no smart pointers, no reference counting. This makes parsing fast and allocation trivial.

The string pool is the second key decision. Every identifier and every literal string in a parse is interned: the same string appears once in the pool, and every reference is a handle to that one copy. This makes comparison O(1) (compare handles, not characters) and memory usage lower.

---

## 4. Layer 2 — `parser`

The `parser` layer turns `.lucid` text into an AST.

### 4.1 Responsibilities

- Lex source text into a stream of tokens.
- Capture trivia (comments, whitespace) between tokens.
- Parse the token stream into an AST according to the grammar.
- Report syntax errors with source locations.
- Recover from errors so that multiple errors can be reported per parse.

### 4.2 Key components

**`Lexer.hpp` / `Lexer.cpp`** — the lexer. Reads UTF-8 text, produces tokens. Handles identifiers, keywords, literals, punctuation, and comments.

**`TriviaScanner.hpp` / `TriviaScanner.cpp`** — captures comments and whitespace between tokens. Attaches them to the following token as trivia. The formatter uses this to preserve user comments.

**`TokenStream.hpp` / `TokenStream.cpp`** — a stream over tokens with lookahead. Provides `peek(n)`, `consume()`, `match(kind)`, and similar operations.

**`ParserContext.hpp`** — the parser's state: the token stream, the arena, the string pool, the diagnostic stack.

**`Parser.hpp` / `Parser.cpp`** — the top-level parser entry point. `parse(text) → Result<ModuleAST, Diagnostics>`.

**`rules/ParseDecl.cpp`** — parses top-level declarations.

**`rules/ParseType.cpp`** — parses `type_id`.

**`rules/ParseValue.cpp`** — parses the four value forms.

**`rules/ParseNode.cpp`** — parses `node_expr` and `trigger_list`.

**`rules/ParseComposite.cpp`** — parses a composite body.

**`support/ErrorRecovery.hpp`** — the error recovery strategy. When a parse error occurs, the parser synchronizes on the next plausible token and continues.

**`support/GrammarPositions.hpp` / `.cpp`** — a table of "which declarations can appear at which positions," used for error recovery and for error messages.

### 4.3 Design notes

The parser is **recursive descent**. There is no parser generator, no table-driven parser, no LALR machinery. The grammar is LL(1), and recursive descent is the simplest way to implement it.

The parser does **not** build a symbol table. It does not resolve names. It does not check types. It produces a syntactic tree. Everything semantic is deferred to `sema`.

The parser does **not** know about the registry. It has no idea what node types exist. A `node_decl` with a `NodeType` of `GarbageNode` parses correctly; `sema` is what rejects it.

Error recovery is deliberate. A parse error should produce a diagnostic and then skip to the next likely declaration, so that a file with three typos reports three errors, not one. The recovery rules are documented in `support/ErrorRecovery.hpp`.

Trivia is captured, not discarded. Every token carries its preceding trivia (comments and whitespace). The parser ignores it; the formatter uses it to preserve comments when rewriting.

---

## 5. Layer 3 — `formatter`

The `formatter` layer turns an AST back into canonical `.lucid` text.

### 5.1 Responsibilities

- Serialize an AST into a string.
- Apply consistent indentation, spacing, and line breaking.
- Preserve user comments.
- Be idempotent: formatting a formatted file produces the same file.

### 5.2 Key components

**`Formatter.hpp` / `Formatter.cpp`** — the entry point. `format(ModuleAST) → std::string`.

**`FormatOptions.hpp` / `FormatOptions.cpp`** — tunables: indent width (spaces vs. tabs), line length, blank-line policy.

**`CommentAttacher.hpp` / `CommentAttacher.cpp`** — walks the AST and attaches comments from trivia to the nearest appropriate AST node.

**`FormatterError.hpp`** — errors the formatter can produce (e.g., unresolvable trivia).

**`layout/IndentWriter.hpp` / `.cpp`** — writes text with an indentation level. Handles the mechanics of indentation and line prefixes.

**`layout/LineBreaker.hpp` / `.cpp`** — decides where to break long declarations across lines. Uses a budget based on `FormatOptions::line_length`.

**`layout/LayoutBuilder.hpp` / `.cpp`** — builds the final output by walking the AST and emitting text through the `IndentWriter`, consulting the `LineBreaker`.

### 5.3 Design notes

The formatter is **not required** for the core use case. The engine can read `.lucid` files without formatting them. The formatter exists for tooling: a `lucid-fmt` tool, an editor's "format on save," a CI check.

The formatter is **not a pretty-printer for source-preserving edits**. It produces canonical output, not minimal edits. If the user wants to preserve their exact formatting, they shouldn't run the formatter.

Idempotence is a hard requirement. If `format(format(x)) != format(x)`, there's a bug. This is tested by the `test_formatter_idempotent.cpp` test.

Comment preservation is best-effort. Comments attached to tokens are reattached to the nearest AST node. Comments in unusual positions (inside a line that's been reformatted, at the end of a file with no following token) may not be preserved. The policy is documented in `CommentAttacher.hpp`.

---

## 6. Layer 4 — `sema`

The `sema` layer takes an AST and a registry and produces a validated `Graph`. This is the semantic analysis pass. It is where most of the format's rules are enforced.

### 6.1 Responsibilities

1. **Resolve imports.** Load every imported module, recursively. Reject cycles.
2. **Collect symbols.** Build a symbol table for each module.
3. **Type-check.** Every argument matches its slot. Every field access resolves. Every `on` names a trigger source.
4. **Enforce `Event` rules.** No `Event` in a value position, a resource field, a composite input, or a node argument (§2.9).
5. **Expand composites.** Inline every composite use. Rename internal nodes and resources to unique names.
6. **Detect cycles.** Value nodes must be acyclic. Composite references must be acyclic.
7. **Detect dead code.** Value nodes must be used. Action nodes must have `on`.
8. **Compute execution order.** Precompute `phase_order` for action nodes, and `value_order` for value nodes.
9. **Produce a `Graph`.** A flat, typed, acyclic structure the engine can walk.

### 6.2 Key components

**`Sema.hpp` / `Sema.cpp`** — the entry point. `analyze(ModuleAST, Registry) → Result<Graph, Diagnostics>`.

**`ImportResolver.hpp` / `.cpp`** — loads imported modules. Caches loaded modules. Handles the import graph.

**`SymbolTable.hpp` / `.cpp`** — per-module symbol table. Maps names to declarations.

**`TypeChecker.hpp` / `.cpp`** — checks arguments against ports, resolves field accesses, enforces `Event` rules.

**`CompositeExpander.hpp` / `.cpp`** — inlines composites. Renames internal nodes and resources.

**`CycleDetector.hpp` / `.cpp`** — checks for cycles among value nodes and among composite references.

**`DeadCodeChecker.hpp` / `.cpp`** — checks that value nodes are used and action nodes have `on`.

**`ExecutionOrder.hpp` / `.cpp`** — computes `phase_order` and `value_order`.

**`Graph.hpp`** — the `Graph` structure, shared between `sema` and the engine.

### 6.3 Design notes

`sema` is the largest layer. It's where the format's rules live. It's also the layer that depends on the registry, and therefore the layer that connects the format library to the engine.

`sema` is **two-pass**:

- **Pass 1 — collect.** Walk the AST and build the symbol table. No references are resolved.
- **Pass 2 — resolve.** Walk the AST again and resolve every reference against the symbol table.

The two-pass model is what makes name resolution order-independent (§4.3 of the grammar). A name can reference a declaration that appears later in the source, because Pass 1 has already collected all names.

Composite expansion happens after Pass 2. Every composite use is inlined, and the resulting graph is what the engine sees. The engine never sees a composite; it only sees nodes.

Cycle detection runs after composite expansion. A cycle among value nodes in the expanded graph is an error.

Execution order is computed last. `phase_order` sorts action nodes by (phase, declaration order). `value_order` is a topological sort of value nodes.

---

## 7. Serialization

Serialization is not a layer; it's a pair of functions that convert between a `Graph` and bytes.

### 7.1 Responsibilities

- `serialize(Graph) → bytes`: produce a binary representation of a graph.
- `deserialize(bytes) → Result<Graph, Diagnostics>`: produce a graph from bytes.

### 7.2 Key components

**`serialization/Serializer.hpp` / `.cpp`** — writes a `Graph` to a byte stream.

**`serialization/Deserializer.hpp` / `.cpp`** — reads a byte stream into a `Graph`.

**`serialization/BinaryFormat.hpp`** — the layout of the binary format: header, sections, alignment rules.

**`serialization/TypeIds.hpp`** — the type ID encoding used in the binary format.

### 7.3 Design notes

The binary format is **platform-independent**. All integers are little-endian, all strings are length-prefixed, all arrays are length-prefixed. There are no pointers, no alignment padding, no machine-specific types.

The binary format is **registry-dependent**. It stores numeric node IDs and type IDs. A `.lucgraph` produced by one registry is not necessarily readable by another. The loader checks that every node ID in the file exists in the current registry. If not, it reports a clear error.

Versioning is minimal. The header carries a format version number and a registry fingerprint. If either mismatches, the loader refuses the file. This is deliberate: shipping a `.lucgraph` that doesn't match the engine's registry is a bug, not a runtime condition to handle.

Serialization is not needed during development. A developer working in the editor uses the in-memory `Graph` directly. Serialization is for shipping.

---

## 8. The `Graph` structure

The `Graph` is the shared type between `sema` and the engine. It is the format library's output and the engine's input.

```cpp
struct Graph {
    // Nodes, flat.
    uint32_t      node_count;
    NodeInstance* nodes;

    // Resources, flat.
    uint32_t      resource_count;
    Resource*     resources;

    // Enums used by the graph.
    uint32_t      enum_count;
    EnumUsage*    enums;

    // Execution order for action nodes.
    uint32_t      phase_order_count;
    NodeIndex*    phase_order;

    // Evaluation order for value nodes.
    uint32_t      value_order_count;
    NodeIndex*    value_order;

    // String pool for literal strings.
    uint32_t      string_pool_size;
    char*         string_pool;
};

struct NodeInstance {
    NodeId       type_id;
    NodeKind     kind;
    Phase        phase;
    uint32_t     arg_count;
    Arg*         args;
    uint32_t     subscriber_count;
    NodeIndex*   subscribers;
};

struct Arg {
    ArgKind   kind;
    union {
        Literal  literal;      // inline literal value
        NodeRef  node_ref;     // reference to another node's output
        ResRef   resource_ref; // reference to a resource field
    };
};
```

The `Graph` is what the engine walks. It has no parser types, no AST, no strings other than literals. It's a self-contained, serializable data structure.

The engine's `GraphExecutor` consumes a `Graph` and runs it. The engine's `Registry` is separate — it describes the node types that the `Graph`'s `NodeId`s refer to.

---

## 9. Public API

The library exposes a small, focused API.

```cpp
// lucid_format/lucid_format.hpp

namespace lucid {

// ─── Compilation ──────────────────────────────────────────────

struct CompileOptions {
    // Reserved for future use.
};

struct CompileResult {
    bool                  ok;
    std::unique_ptr<Graph> graph;         // valid if ok
    std::vector<Diagnostic> diagnostics;  // always populated
};

CompileResult compile(
    std::string_view  source,
    std::string_view  filename,
    const Registry&   registry,
    CompileOptions    options = {}
);

// ─── Formatting ───────────────────────────────────────────────

struct FormatOptions {
    int  indent_width  = 4;
    int  line_length   = 100;
    bool use_tabs      = false;
};

struct FormatResult {
    bool                    ok;
    std::string             text;        // valid if ok
    std::vector<Diagnostic> diagnostics; // always populated
};

FormatResult format(
    std::string_view source,
    std::string_view filename,
    FormatOptions    options = {}
);

// ─── Serialization ────────────────────────────────────────────

std::vector<uint8_t> serialize(const Graph& graph);

struct DeserializeResult {
    bool                    ok;
    std::unique_ptr<Graph>  graph;        // valid if ok
    std::vector<Diagnostic> diagnostics;  // always populated
};

DeserializeResult deserialize(
    std::span<const uint8_t> bytes,
    const Registry&          registry
);

// ─── The registry interface ───────────────────────────────────

class Registry { /* ... */ };

} // namespace lucid
```

Four functions: `compile`, `format`, `serialize`, `deserialize`. Plus the `Registry` interface and the `Graph` structure.

That's the entire public surface. Everything else is internal and can change without notice.

---

## 10. Dependency rules

To keep the library maintainable, dependencies between layers are constrained:

| Layer           | May depend on    | May not depend on                                       |
| --------------- | ---------------- | ------------------------------------------------------- |
| `core`          | (nothing)        | everything                                              |
| `parser`        | `core`           | `formatter`, `sema`, `serialization`                    |
| `formatter`     | `core`, `parser` | `sema`, `serialization`                                 |
| `sema`          | `core`, `parser` | `formatter`, `serialization`                            |
| `serialization` | `core`           | `parser`, `formatter`, `sema` (except the `Graph` type) |

`serialization` depends on `sema`'s output type (`Graph`) but not on `sema` itself. The `Graph` type is shared; it lives in `core` in a file both layers include, or in its own small header.

These rules are enforced by the build system. Each layer is its own CMake target with explicit dependencies. A violation is a link error.

---

## 11. Build layout

The library is built as several CMake targets, layered.

```
lucid_core          (static library)
    ↑
lucid_parser        (static library)
    ↑
lucid_formatter     (static library)
lucid_sema          (static library)     ← both depend on parser
    ↑
lucid_serialization (static library)     ← depends on core, shares Graph
    ↑
lucid_format        (interface library)  ← umbrella target; pulls everything together
```

The `lucid_format` interface target exists so that consumers can link one target and get everything. Behind the scenes, they may link only the parts they need (`lucid_parser` for a formatter tool, `lucid_serialization` for a shipping runtime).

The `sema/` directory in the draft layout is marked **DEFERRED**. This is correct: build `core`, `parser`, and `formatter` first. Add `sema` when the parser and formatter are working. Add `serialization` after `sema`.

---

## 12. What's deferred

The draft file structure correctly marks `sema/` as deferred. This is the right call. The order of implementation is:

1. **`core`** — tokens, AST types, arena, string pool, diagnostics, trivia. Nothing depends on anything else, and everything depends on it.
2. **`parser`** — lexer, then parser. Produces an AST from text. Testable with fixtures.
3. **`formatter`** — AST back to text. Testable by round-tripping: parse, format, compare with the original (or with expected output).
4. **`sema`** — AST + registry to `Graph`. Deferred until 1–3 are working, because Sema's design will be informed by what the parser and formatter actually need.
5. **`serialization`** — `Graph` to bytes and back. Deferred until `sema` produces a stable `Graph`.
6. **`tools`** — CLI utilities (`lucid-fmt`, `lucid-check`). Deferred until the library is stable.

The order is deliberate. Each layer is fully working before the next is started. Each layer is tested in isolation before the next depends on it. The result is a library that is always in a working state — no "it compiles but nothing works" phases.

---

## 13. Testing strategy

Each layer has its own tests. The tests are layered, too.

**`core` tests:**
- String pool interning.
- Arena allocation and freeing.
- Source location tracking.
- Token construction and trivia attachment.

**`parser` tests:**
- Lexer tests for every token kind.
- Parser tests for every declaration kind.
- Value parsing.
- Composite parsing.
- Token stream lookahead.
- Error recovery: malformed input produces diagnostics, not crashes.

**`formatter` tests:**
- Basic formatting of each declaration kind.
- Comment preservation.
- Idempotence: formatting a formatted file produces the same file.
- Round-trip: parse a fixture, format it, compare with the expected output.

**Fixtures** live under `tests/fixtures/`. Each fixture has an `input/` file and an `expected/` file. The formatter test parses the input, formats it, and compares with the expected.

Additional fixture categories can be added as layers come online: `sema` fixtures that parse a file, run Sema, and compare the resulting `Graph` against an expected form.

---

## 14. The bottom line

The library is five layers, each building on the one below it:

- **`core`** — vocabulary. Tokens, AST types, memory, diagnostics.
- **`parser`** — text to AST.
- **`formatter`** — AST to text.
- **`sema`** — AST + registry to `Graph`.
- **`serialization`** — `Graph` to bytes.

Plus two cross-cutting pieces:

- The **`Graph` structure** — the output of `sema`, the input of the engine.
- The **`Registry` interface** — the way the engine tells the library what node types exist.

The public API is four functions: `compile`, `format`, `serialize`, `deserialize`. Everything else is internal.

Dependencies flow upward only. Each layer is testable in isolation. Each layer is fully working before the next is started.

The build order matches the dependency order: `core`, then `parser`, then `formatter`, then `sema`, then `serialization`, then tools.

The `sema/` directory is correctly marked deferred. Build the first three layers first. Everything else follows.