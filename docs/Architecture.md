# Lucid Format Library — Architecture

The format library, `lucid_format`, reads, validates, and writes
Lucid graph files. This document describes its architecture: the
layers, the components, and how they fit together.

---

## 1. Purpose and scope

`lucid_format` is a **standalone library**. It has no dependency on
the engine, the renderer, the physics backend, or any platform API.
It is pure C++17.

It performs four jobs:

1. **Lex and parse** a `.lucid` file into an abstract syntax tree
   (AST).
2. **Format** an AST back into canonical `.lucid` text.
3. **Validate** an AST against a host-provided registry (this is
   Sema).
4. **Serialize and deserialize** a validated graph into and out of
   a `.lucgraph` binary.

It exposes a small API. Everything else is internal.

The library is designed to be usable by:

- The **engine**, at runtime, for loading graphs.
- The **editor**, for validation and preview.
- The **build tool**, for producing `.lucgraph` files.
- **Tools** — linters, formatters, visualizers, CI validators.

Each of these uses a subset of the library. The editor uses
everything. A formatter tool uses only the parser and the
formatter. A shipping game uses only the deserializer.

---

## 2. The layers

The library is organized into six layers, each building on the one
below it.

```
┌──────────────────────────────────────────────────────────┐
│  cli           ── the `lucid` command-line tool          │
├──────────────────────────────────────────────────────────┤
│  serialization ── Graph ↔ .lucgraph bytes                │
├──────────────────────────────────────────────────────────┤
│  sema          ── AST + Registry → Graph                 │
├──────────────────────────────────────────────────────────┤
│  formatter     ── AST → canonical text                   │
├──────────────────────────────────────────────────────────┤
│  parser        ── tokens → AST                           │
├──────────────────────────────────────────────────────────┤
│  core          ── tokens, AST types, diagnostics, memory │
└──────────────────────────────────────────────────────────┘
```

Plus two cross-cutting pieces that sit alongside the layers:

- **The `Graph` structure** — the output of Sema, the input of the
  engine, and the payload of the serialization layer.
- **The `Registry` interface** — the engine's declaration of node
  types, enum types, handle types, and phases. Sema reads it.

Dependencies flow upward only. `core` depends on nothing. `parser`
depends on `core`. `formatter` depends on `core` and `parser`.
`sema` depends on `core` and `parser`. `serialization` depends on
`core` and `sema`. `cli` depends on everything. Nothing depends on
the engine.

---

## 3. Layer 1 — `core`

The `core` layer contains the pieces every other layer needs:
tokens, AST node types, diagnostics, memory management, source
locations, and trivia.

### 3.1 Responsibilities

- Define token kinds and the `Token` structure.
- Define all AST node types.
- Provide source location tracking.
- Provide an arena allocator for AST nodes.
- Provide a string pool for interned strings.
- Define the diagnostic system.
- Capture trivia (comments and whitespace) attached to tokens.

### 3.2 Key components

**`SourceLocation.hpp`** — a position in a source file: byte
offset, line, column. Attached to every token and every AST node.

**`Tokens.hpp`** — the `Token` structure and the `TokenType` enum.
A token is a value type: kind, lexeme (an `InternedString`), source
location, and attached trivia.

**`ast/BaseAST.hpp`** — the root of the AST hierarchy. Defines
`BaseAST`, the `ASTKind` enum, the `isa<T>()` / `as<T>()` helpers,
and the `AST_ASSERT_MSG` macro.

**`ast/DeclAST.hpp`** — the four declaration kinds:
`ImportDeclAST`, `EnumDeclAST`, `ResourceDeclAST`, `NodeDeclAST`.
Plus the two child nodes `EnumMemberAST` and `ResourceFieldAST`.

**`ast/ValueAST.hpp`** — the four value forms:
`LiteralValueAST`, `IdentifierValueAST`, `FieldAccessValueAST`,
`InlineNodeValueAST`. Plus `NodeExprAST`, the shape a node
expression has in two positions.

**`ast/TypeAST.hpp`** — `TypeIdAST`, a type reference: an
optional module qualifier and a name.

**`ast/AttributeAST.hpp`** — `AttributeAST`, one `@name` prefix.

**`ast/ModuleAST.hpp`** — the root of a parsed file: a file path, a
span of top-level declarations, and an `hasErrors` flag.

**`memory/ASTArena.hpp`** — a bump allocator that owns all AST
nodes for one parse. Frees everything at once. Also hosts
`ArenaSpan`'s `SpanBuilder` helpers.

**`memory/ArenaSpan.hpp`** — a lightweight, non-owning view of a
contiguous run of elements. Used for every list of child nodes.

**`memory/InternedString.hpp`** — a 4-byte handle to a string in a
`StringPool`. Comparison is an integer comparison; hashing is
trivial.

**`memory/StringPool.hpp`** — internes strings for one compilation
session. Owns the bytes; hands out stable `InternedString` handles.

**`diagnostics/DiagCode.hpp`** — the enum of every diagnostic the
library can emit, organized into bands by pipeline stage.

**`diagnostics/Diagnostic.hpp`** — the diagnostic engine: collects
diagnostics, provides `hasErrors()`, formats them for display.

**`trivia/Trivia.hpp`** and **`trivia/TriviaBuffer.hpp`** — the
comments captured by the lexer, in source order. Used by the
formatter to re-emit comments.

### 3.3 Design notes

The `core` layer has no knowledge of grammar structure beyond the
AST node types. It does not know how to build an AST (that is the
parser) or what to do with one (that is `sema` or the formatter).
It defines the *vocabulary*, not the *sentences*.

The AST arena is the key memory decision. A parse allocates all AST
nodes from a single arena. When the parse is finished, the arena is
freed in one operation. There is no per-node deletion, no smart
pointers, no reference counting.

The string pool is the second key decision. Every identifier and
every literal string in a parse is interned: the same string
appears once in the pool, and every reference is a handle to that
one copy. Comparison is O(1); memory use is lower.

---

## 4. Layer 2 — `parser`

The `parser` layer turns `.lucid` text into an AST.

### 4.1 Responsibilities

- Lex source text into a stream of tokens.
- Capture trivia (comments, whitespace) between tokens.
- Parse the token stream into an AST according to the grammar.
- Report syntax errors with source locations.
- Recover from errors so multiple errors are reported per parse.

### 4.2 Key components

**`lexer/Lexer.hpp`** — the lexer. Reads UTF-8 text and produces
tokens. Handles identifiers, keywords, literals, punctuation, and
comments.

The lexer has two entry points: `tokenize`, which skips comments
and is used by the parser, and `tokenizeWithTrivia`, which collects
comments into a `TriviaBuffer` and is used by the formatter.

**`context/ParserContext.hpp`** — the parser's per-session state:
the string pool, the arena, the diagnostic engine, the current
file, and the token stream.

**`context/TokenStream.hpp`** — a stream over tokens with
lookahead. Provides `peek(n)`, `consume()`, `match(kind)`, and
similar operations.

**`Parser.hpp`** / **`Parser.cpp`** — the parser's entry point.
`parseFile(path, source, ctx) → ModuleAST*`. The returned module is
never null; a file that could not be parsed still produces a
`ModuleAST` with `hasErrors == true`.

**`rules/ParseDecl.cpp`** — parses the four top-level declaration
kinds and their attribute prefixes.

**`rules/ParseType.cpp`** — parses `type_id`.

**`rules/ParseValue.cpp`** — parses the four value forms.

**`rules/ParseNode.cpp`** — parses `node_expr`, `arg_list`, and
`trigger_list`.

**`support/ErrorRecovery.hpp`** — the error recovery strategy. On
a parse error, the parser synchronizes on the next plausible token
and continues.

**`support/GrammarPositions.hpp`** — predicates that identify the
start-set of each production, used by error recovery.

**`dump/JSONDumper.hpp`** / **`dump/JSONWriter.hpp`** — a
debugging and testing aid: an AST → JSON dump. Used by fixture
tests to verify the parser's output.

### 4.3 Design notes

The parser is **recursive descent**. There is no parser generator,
no table-driven parser, no LALR machinery. The grammar is LL(1),
and recursive descent is the simplest way to implement it.

The parser does **not** build a symbol table. It does not resolve
names. It does not check types. It produces a syntactic tree.
Everything semantic is deferred to `sema`.

The parser does **not** know about the registry. It has no idea
what node types exist. A `node_decl` whose type is `GarbageNode`
parses correctly; `sema` is what rejects it.

Error recovery is deliberate. A parse error produces a diagnostic
and then skips to the next likely declaration, so a file with three
typos reports three errors, not one.

Trivia is captured, not discarded. Every token carries its
preceding trivia. The parser ignores it; the formatter uses it to
preserve comments when rewriting.

---

## 5. Layer 3 — `formatter`

The `formatter` layer turns an AST back into canonical `.lucid`
text.

### 5.1 Responsibilities

- Serialize an AST into a string.
- Apply consistent indentation, spacing, and line breaking.
- Preserve user comments.
- Be idempotent: formatting a formatted file produces the same
  file.

### 5.2 Key components

**`Formatter.hpp`** / **`Formatter.cpp`** — the entry points.
`format(source, filename) → FormatResult` is the public API;
`formatModule(module, source, pool, options) → std::string` is the
lower-level path for callers that already have an AST.

**`FormatOptions.hpp`** — two tunables: indent width and
tabs-vs-spaces.

**`IndentWriter.hpp`** — the text emitter. Tracks the current
indent level and column; writes the indent lazily on the first
`write()` after a `newline()`.

**`src/formatter/LayoutBuilder.hpp`** / **`LayoutBuilder.cpp`** —
the AST → text walker. Walks the module, emits declarations,
interleaves comments.

### 5.3 Design notes

The formatter is **not required** for the engine's use case. The
engine reads `.lucgraph` files without formatting them. The
formatter exists for tooling: `lucid format`, an editor's
"format on save", a CI check.

The formatter is **not a source-preserving editor**. It produces
canonical output, not minimal edits. A caller that wants to
preserve the user's exact formatting should not run the formatter.

Idempotence is a hard requirement. If `format(format(x))` is not
equal to `format(x)`, there is a bug. This is tested by the
formatter fixtures.

Comment preservation is best-effort. Comments attached to tokens
are reattached to the nearest AST node. Comments in unusual
positions may not be preserved. The policy is documented in the
formatter's source.

---

## 6. Layer 4 — `sema`

The `sema` layer takes an AST and a registry and produces a
validated `Graph`. It is where the language's rules are enforced.

### 6.1 Responsibilities

1. **Resolve imports.** Load every imported module, recursively.
   Reject cycles.
2. **Collect symbols.** Build a symbol table for each module.
3. **Type-check.** Every argument matches its slot. Every field
   access resolves. Every `on` names a trigger node.
4. **Enforce kind-based rules.** An `on` target must be a Trigger;
   an Action node must have at least one `on`.
5. **Detect cycles.** Value nodes must be acyclic.
6. **Detect dead code.** Value nodes must be used.
7. **Compute execution order.** Precompute `phase_order` for
   action nodes, and `value_order` for value nodes.
8. **Produce a `Graph`.** A flat, typed, acyclic structure the
   engine can walk.

### 6.2 Key components

**`Sema.hpp`** / **`Sema.cpp`** — the entry points.
`compile(source, filename, registry, options) → CompileResult`;
`compileModule(module, source, filename, pool, registry, options)`
for the same operation without the parse.

**`Registry.hpp`** — the interface between the engine and the
library. Node types, enum types, handle types, and phases. Sema
reads it; the engine builds it. Also declares
`computeRegistryFingerprint`.

**`Graph.hpp`** — the `Graph` structure. Flat vectors of
`NodeInstance`, `Arg`, `Resource`, and `ResourceField`, plus two
execution orders, a string pool, and the registry fingerprint.

**`TypeId.hpp`** / **`Literal.hpp`** / **`NodeKind.hpp`** — the
types that describe a value and a node.

**`src/sema/SymbolCollector.hpp`** — Pass 1: build the per-module
symbol table.

**`src/sema/Resolver.hpp`** — Pass 2: resolve every name against
the symbol table.

**`src/sema/TypeChecker.hpp`** — Pass 3: assign types, check
consistency, fold constants, and enforce the trigger rules.

**`src/sema/GraphBuilder.hpp`** — Pass 4: build the `Graph` from
the resolved, checked modules.

**`src/sema/DeadCodeChecker.hpp`** — Pass 5: report unused value
nodes.

**`src/sema/ImportResolver.hpp`** — load a module's transitive
imports, in dependency order.

**`src/sema/AttributeChecker.hpp`** — validate `@export` and other
attributes.

**`src/sema/ModuleTable.hpp`** — the map from a module's name to
its symbol table, used to resolve `::`-qualified references.

**`src/sema/SymbolTable.hpp`** — the per-module symbol table.

**`src/sema/ResolutionMap.hpp`** — Pass 2's output: what each
reference resolved to.

**`src/sema/TypeMap.hpp`** — Pass 3's output: what type each
expression has.

**`src/sema/ConstantValueMap.hpp`** — Pass 3's output: the
compile-time value of each reducible expression.

**`src/sema/dump/GraphDumper.hpp`** — serialize a `Graph` to JSON.
Used by fixture tests.

### 6.3 Design notes

`sema` is the largest layer. It is where the language's rules
live, and it is the layer that depends on the registry — and
therefore the layer that connects the format library to the engine.

`sema` is **two-pass** for name resolution:

- **Pass 1 — collect.** Walk the AST and build the symbol table.
  No references are resolved.
- **Pass 2 — resolve.** Walk the AST again and resolve every
  reference against the symbol table.

The two-pass model is what makes name resolution order-independent:
a name can reference a declaration that appears later in the
source, because Pass 1 has already collected all names.

The remaining passes run in order: type-check, build the graph,
detect dead code. If any pass reports an error, subsequent passes
are skipped.

Multi-module support: `sema` loads a root module's transitive
imports, runs Passes 1–3 on each module, merges the exported
symbols into each importer's symbol table, and builds a single
`Graph` from all of them.

The registry fingerprint is computed once, at the end of graph
construction, and stored in the `Graph`. It identifies the
registry the graph was compiled against. The serialization layer
writes it into the file; the deserializer checks it.

---

## 7. Layer 5 — `serialization`

The `serialization` layer converts between a `Graph` and
`.lucgraph` bytes.

### 7.1 Responsibilities

- `serialize(Graph) → bytes`: produce a binary representation of a
  graph.
- `deserialize(bytes, registry) → DeserializeResult`: produce a
  graph from bytes, validating the header and the registry
  fingerprint.

### 7.2 Key components

**`Serializer.hpp`** / **`Serializer.cpp`** — writes a `Graph` to
a byte vector.

**`Deserializer.hpp`** / **`Deserializer.cpp`** — reads a byte
buffer into a `Graph`.

**`BinaryFormat.hpp`** — the layout of the binary format: the
header, the section table, the eight sections, and every wire
constant (magic, format version, section IDs, arg kinds, literal
kinds, type kinds).

**`ByteSpan.hpp`** — a read-only view over a contiguous byte
buffer. On C++20 this collapses to `std::span<const uint8_t>`; on
C++17 it is a small local type.

**`src/serialization/Reader.hpp`** — the low-level byte reader
used by the deserializer.

**`src/serialization/Writer.hpp`** — the low-level byte writer
used by the serializer.

### 7.3 Design notes

The binary format is **platform-independent**. All integers are
little-endian; sections are packed with no alignment padding; every
variable-length datum is length-prefixed. No pointers, no
machine-specific types.

The binary format is **registry-dependent** in the sense that it
stores numeric node IDs. The header carries the registry
fingerprint. The loader checks it before reading any section, and
refuses a file compiled against a different registry.

The format is **versioned**. The header carries a `format_version`
field. A future change to the byte layout increments it. A loader
that does not recognize the version refuses the file.

The format has **eight sections**, all required in version 1, each
listed in a section table that carries its ID and size. The loader
dispatches on ID, not position, which makes the format
forward-compatible within a version.

Serialization is not needed during development. A developer working
in the editor uses the in-memory `Graph` directly. Serialization
is for shipping.

The complete byte layout is documented in
[`docs/notes/BinaryFormat.md`](notes/BinaryFormat.md) (or, for now,
in the source headers under `serialization/include/serialization/`).

---

## 8. Layer 6 — `cli`

The `cli` layer is the command-line interface: the `lucid` binary,
its subcommands, and the shared infrastructure they use.

### 8.1 Responsibilities

- Parse command-line arguments into a `CLIOptions`.
- Provide a default registry the CLI tools share.
- Dispatch to a subcommand.
- Read source files, run the pipeline, write output files.

### 8.2 Key components

**`CLIOptions.hpp`** / **`CLIOptions.cpp`** — the parsed
command-line options. A two-layer parse: read the subcommand first,
then its options.

**`Version.hpp`** — the CLI's version string, one constant so the
subcommands cannot drift.

**`Registry.hpp`** / **`Registry.cpp`** — the default registry the
CLI tools use. A small fixed vocabulary: enough to compile the
fixtures and examples, not enough to run a real game. A real
engine supplies its own registry.

**`src/cli/Main.cpp`** — the `lucid` entry point and subcommand
dispatcher. Handles top-level `--help` and `--version`, dispatches
to a `run*` function based on `opts.subcommand`.

**`src/cli/commands/Format.hpp`** / **`Format.cpp`** — the
`lucid format` subcommand.

**`src/cli/commands/Check.hpp`** / **`Check.cpp`** — the
`lucid check` subcommand.

**`src/cli/commands/Compile.hpp`** / **`Compile.cpp`** — the
`lucid compile` subcommand.

### 8.3 The unified tool

The CLI is a single executable, `lucid`, with three subcommands:

```
lucid format  [options] [file]
lucid check   [options] [file]
lucid compile [options] [file]
```

A single binary is easier to ship, document, and extend. A new
subcommand (a future `lucid lsp`, a `lucid info` that inspects a
`.lucgraph`) is a new `run*` function and a new arm in the
dispatcher's switch.

The subcommands share:

- The argument parser (`CLIOptions`).
- The default registry (`defaultRegistry()`).
- The version string (`kVersion`).
- The output and diagnostic conventions (exit 0 on success, 1 on a
  compile error, 2 on an I/O or usage error).

### 8.4 What the CLI does not do

The CLI does not implement the pipeline. It calls into the
library: `formatter::format`, `sema::compile`,
`serialization::serialize`. Every stage the CLI touches is also
available to any C++ program that links the library directly. The
CLI is a convenience wrapper, not a separate code path.

---

## 9. The `Graph` structure

The `Graph` is the shared type between `sema` and the engine. It is
the format library's output and the engine's input.

```cpp
struct Graph
{
    std::vector<NodeInstance>    nodes;
    std::vector<Arg>             args;
    std::vector<Resource>        resources;
    std::vector<ResourceField>   resource_fields;
    std::vector<NodeIndex>       subscribers;
    std::vector<NodeIndex>       phase_order;
    std::vector<NodeIndex>       value_order;
    std::vector<char>            string_pool;

    // The boundary between the graph's literals and its interned names.
    uint32_t literal_pool_size = 0;

    uint64_t registry_fingerprint = 0;
};

struct NodeInstance
{
    NodeId   type_id;
    uint32_t phase;
    uint32_t args_offset;
    uint32_t args_count;
    uint32_t subscribers_offset;
    uint32_t subscribers_count;
};

struct Arg
{
    enum class Kind : uint8_t { Literal, NodeRef, ResourceRef };
    Kind kind;
    union {
        Literal     literal;
        NodeIndex   node_ref;
        struct {
            uint32_t resource_index;
            uint32_t field_index;
        } resource_ref;
    };
};
```

The `Graph` is what the engine walks. It has no parser types, no
AST, no strings other than literals and resource names. It is a
self-contained, serializable data structure.

The flat-vector layout is deliberate. Every list is one vector. A
node slices the shared `args` and `subscribers` vectors with
`(offset, count)` pairs. A resource slices `resource_fields` the
same way. One allocation per vector, not one per node.

The engine's `GraphExecutor` consumes a `Graph` and runs it. The
engine's `Registry` is separate — it describes the node types that
the graph's `NodeId`s refer to. See
[`RegistryContractNote.md`](notes/RegistryContractNote.md) for the
full boundary.

---

## 10. Public API

The library exposes a small, focused API:

```cpp
namespace lucid::sema
{
    struct CompileOptions;
    struct CompileResult;

    CompileResult compile(
        std::string_view  source,
        std::string_view  filename,
        const Registry&   registry,
        CompileOptions    options = {});
}

namespace lucid::formatter
{
    struct FormatOptions;
    struct FormatResult;

    FormatResult format(
        std::string_view source,
        std::string_view filename,
        FormatOptions    options = {});
}

namespace lucid::serialization
{
    std::vector<uint8_t> serialize(const lucid::sema::Graph& graph);

    struct DeserializeResult;

    DeserializeResult deserialize(
        ByteSpan                bytes,
        const lucid::sema::Registry& registry);
}
```

Four functions: `compile`, `format`, `serialize`, `deserialize`.
Plus the `Registry` interface, the `Graph` structure, and the
`ByteSpan` view.

That is the entire public surface. Everything else is internal and
can change without notice.

---

## 11. Dependency rules

To keep the library maintainable, dependencies between layers are
constrained:

| Layer           | May depend on    | May not depend on                           |
| --------------- | ---------------- | ------------------------------------------- |
| `core`          | (nothing)        | everything                                  |
| `parser`        | `core`           | `formatter`, `sema`, `serialization`, `cli` |
| `formatter`     | `core`, `parser` | `sema`, `serialization`, `cli`              |
| `sema`          | `core`, `parser` | `formatter`, `serialization`, `cli`         |
| `serialization` | `core`, `sema`   | `parser`, `formatter`, `cli`                |
| `cli`           | everything       | —                                           |

`serialization` depends on `sema`'s output type (`Graph`) and on
`Registry`. `cli` depends on everything because its three
subcommands each wrap one stage of the pipeline.

These rules are enforced by the build system. Each layer is its own
CMake target with explicit dependencies. A violation is a link
error.

---

## 12. Build layout

The library is built as several CMake targets, layered.

```
lucid_core            (static library)
    ↑
lucid_parser          (static library)
    ↑
lucid_formatter       (static library)
lucid_sema            (static library)     ← both depend on parser
    ↑
lucid_serialization   (static library)     ← depends on core, sema
    ↑
lucid_cli             (static library)     ← depends on everything
    ↑
lucid                 (executable)         ← the unified CLI
```

The six libraries exist so that a consumer can link only what it
needs:

- A shipping runtime that only loads `.lucgraph` files links
  `lucid_serialization` (which pulls in `lucid_sema` for the
  `Graph` type, and `lucid_core`).
- A formatter tool links `lucid_parser` and `lucid_formatter`.
- The engine's editor links everything.

There is no umbrella target. A consumer includes the headers it
uses.

---

## 13. Testing strategy

Each layer has its own tests. The tests are layered too.

**`core` tests** — string pool interning, arena allocation and
freeing, source location tracking, token construction, trivia
attachment.

**`parser` tests** — lexer tests for every token kind, parser tests
for every declaration kind, value parsing, error recovery,
fixture-driven tests that compare the AST JSON dump against a
stored expected file.

**`formatter` tests** — basic formatting of each declaration kind,
comment preservation, idempotence, and round-trip fixture tests.

**`sema` tests** — symbol collection, name resolution, type
checking, graph construction, dead-code detection, import
resolution, multi-module compiles, and fixture-driven tests that
compare the graph JSON dump against a stored expected file.

**`serialization` tests** — byte-level structure tests for
`serialize`, round-trip tests for `deserialize(serialize(g))`, and
error-path tests for malformed files.

**`cli` tests** — argument-parsing tests, per-command tests that
exercise the commands' exit codes, and integration tests that
invoke the built `lucid` binary as a subprocess.

**Fixtures** live under `tests/fixtures/`. Each fixture has an
input file and an expected output. The formatter and Sema fixtures
are regenerated by a dedicated tool (`lucid-fixture-regen`); the
parser fixtures are hand-written and verified.

---

## 14. The bottom line

The library is six layers, each building on the one below it:

- **`core`** — vocabulary. Tokens, AST types, memory, diagnostics.
- **`parser`** — text to AST.
- **`formatter`** — AST to text.
- **`sema`** — AST + registry to `Graph`.
- **`serialization`** — `Graph` to bytes and back.
- **`cli`** — the `lucid` command-line tool.

Plus two cross-cutting pieces:

- The **`Graph` structure** — the output of `sema`, the input of
  the engine, the payload of `serialization`.
- The **`Registry` interface** — the engine's vocabulary, read by
  `sema`.

The public API is four functions: `compile`, `format`, `serialize`,
`deserialize`. Everything else is internal.

Dependencies flow upward only. Each layer is testable in isolation.
Nothing depends on the engine.