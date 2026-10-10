# Lucid Graph Format

A format library for the Lucid graph language: a small, declarative
language for describing graphs of nodes that an engine executes.

The library reads `.lucid` source, validates it against a
host-provided registry, and produces a `.lucgraph` binary that an
engine loads and runs. It also formats `.lucid` source and reports
diagnostics.

The library is standalone. It depends on nothing but the C++17
standard library. It does not depend on the engine that runs its
output.

---

## What it does

Four jobs, each exposed as one function:

- **Compile** — `compile(source, filename, registry)` parses a
  `.lucid` file, resolves names and types, enforces the language's
  rules, and returns a `Graph`.
- **Format** — `format(source, filename)` parses a `.lucid` file and
  returns canonical text, with comments preserved.
- **Serialize** — `serialize(graph)` writes a `Graph` to a
  `.lucgraph` byte vector.
- **Deserialize** — `deserialize(bytes, registry)` reads a
  `.lucgraph` byte buffer back into a `Graph`, checking the
  registry fingerprint before touching any section.

Everything else is internal.

---

## Building

Requires CMake 3.15+ and a C++17 compiler. On Windows, MSVC 2022 is
required; the build pins it.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Debug
```

The build produces:

- `lucid` — the command-line tool.
- `lucid_core`, `lucid_parser`, `lucid_formatter`, `lucid_sema`,
  `lucid_serialization`, `lucid_cli` — the static libraries.

Tests use [Catch2](https://github.com/catchorg/Catch2), fetched
automatically by CMake. The integration tests use Python 3, if
available; they are skipped otherwise.

---

## Command-line usage

```
lucid <command> [options] [file]
```

### `lucid format`

Format a `.lucid` file into canonical text.

```sh
lucid format hello.lucid           # print to stdout
lucid format -o out.lucid in.lucid # write to a file
lucid format -                     # read from stdin
```

### `lucid check`

Compile a `.lucid` file and report diagnostics. Exit code is 0 on
success, 1 on a compile error, 2 on an I/O or usage error.

```sh
lucid check hello.lucid
```

### `lucid compile`

Compile a `.lucid` file to a `.lucgraph` binary.

```sh
lucid compile hello.lucid                  # writes hello.lucgraph
lucid compile -o out.lucgraph hello.lucid  # writes out.lucgraph
echo "node x = Float32Node(1.0)" | lucid compile -o out.lucgraph -
```

### `lucid --help`

Print the top-level usage. `lucid <command> --help` prints a
command's help.

---

## The language

A `.lucid` file contains declarations. There are four kinds:

```
import module.path

enum Key { W, A, S, D }

resource PlayerConfig {
    speed:      float = 200.0,
    jump_force: float = -400.0,
    max_hp:     int   = 100,
}

node speed = Float32Node(200.0)
node every_frame = EveryFrame()
node printer = PrintNode(speed) on every_frame
```

Key properties:

- **No operators.** Values are literals, identifiers, field
  accesses, and inline node expressions. Arithmetic is
  `AddNode(a, b)`, not `a + b`.
- **No statements.** The top level contains only declarations.
  Control flow is via trigger and action nodes.
- **Order-independent.** A declaration may reference a declaration
  that appears later in the file.
- **Registry-driven.** Every type and every node type comes from
  the engine's registry. The format library has no built-in
  vocabulary beyond the primitive types.

The complete grammar is in [`docs/Grammar.md`](docs/Grammar.md).

---

## The `.lucgraph` format

A `.lucgraph` file is a 32-byte header followed by a section table
and eight sections. Every integer is little-endian; sections are
packed with no alignment padding.

The header carries the format version, a registry fingerprint, and
the section count. The loader checks the fingerprint against its
own registry's before reading any section. A mismatch is a hard
error: the file was compiled against a different registry, and its
numeric node IDs would misbind.

The byte-level constants live in
[`serialization/include/serialization/BinaryFormat.hpp`](serialization/include/serialization/BinaryFormat.hpp).
The format's design is documented in
[`docs/Architecture.md`](docs/Architecture.md) §7.

---

## Using the library

### Compile, serialize, deserialize

```cpp
#include "sema/Sema.hpp"
#include "serialization/Serializer.hpp"
#include "serialization/Deserializer.hpp"

// Build or obtain a Registry (see "The registry" below).
const lucid::sema::Registry& registry = my_engine::registry();

// Compile a source file.
auto result = lucid::sema::compile(source, "player.lucid", registry);
if (!result.ok)
{
    for (const auto& d : result.diagnostics)
    {
        std::cerr << lucid::diag::severityName(d.severity) << ": "
                  << d.message << "\n";
    }
    return 1;
}

// Serialize the graph.
std::vector<uint8_t> bytes =
    lucid::serialization::serialize(*result.graph);
writeFile("player.lucgraph", bytes);

// Later, at load time, in the engine:
std::vector<uint8_t> loaded = readFile("player.lucgraph");
auto dr = lucid::serialization::deserialize(
    lucid::serialization::ByteSpan(loaded), registry);
if (!dr.ok)
{
    // The file was compiled against a different registry, or is
    // corrupt. Report and fail the load.
    return 1;
}

// dr.graph is the runtime input. Walk it, execute it.
```

The compile step returns a `Graph` the engine can run. The
serialize/deserialize pair is how a compiled graph ships: the
build tool writes a `.lucgraph`, the engine reads it back.

### The registry

Every name and every type in a `.lucid` file is resolved against a
`Registry`: a list of node types, enum types, handle types, and
execution phases that the engine provides. The format library does
not know what `MoveBody` does or what a `BodyRef` points at; it
only knows the shapes the registry declares.

The registry's **shape** is defined by the format library:
`NodeTypeInfo`, `EnumTypeInfo`, `HandleTypeInfo`, `PhaseInfo`, and
the `NodeKind` enum all live in `sema/include/sema/`.

The registry's **content** is the engine's. The engine lists its
nodes, their arguments, their kinds, and their phases. The format
library reads this at compile time to check every call site, and at
load time to verify the fingerprint.

A complete, compilable sample — a registry for a small engine, plus
the runtime dispatch table that runs the graph it produces — lives
in
[`docs/notes/RegistryContractNote.md`](docs/notes/RegistryContractNote.md)
§14. Read it if you are writing a registry for the first time.

---

## What the format library does and does not do

**It does:**

- Parse `.lucid` files into an AST.
- Format an AST back into canonical text.
- Validate an AST against a registry: check names, types,
  arguments, and the kind-based rules.
- Produce a `Graph`: a flat, resolved, acyclic structure the engine
  executes.
- Serialize a `Graph` to `.lucgraph` bytes and deserialize it back.

**It does not:**

- Execute the graph. That is the engine's job.
- Know any specific node type. `MoveBody`, `AddNode`, and
  `Float32Node` are names the engine supplies; the library never
  references them.
- Know what a handle points at. The library carries a handle's type
  name and its value; the engine resolves both.
- Own the engine's resources. Handles, bodies, textures, and any
  other engine-side object live in the engine.

The boundary is one data structure — the `Registry` — and one
output — the `Graph`. Everything else is internal.

---

## Layout

```
core/            format-independent infrastructure
                 (tokens, AST, arena, string pool, diagnostics, trivia)
parser/          lexer and parser
formatter/       AST → canonical text
sema/            AST + registry → Graph
serialization/   Graph ↔ .lucgraph bytes
cli/             shared command-line infrastructure
tests/           unit and integration tests
docs/            design documents
```

Dependencies flow upward only. `core` depends on nothing. `parser`
depends on `core`. `formatter` and `sema` depend on `core` and
`parser`. `serialization` depends on `core` and `sema`. `cli`
depends on everything. Nothing depends on the engine.

The public API — the four functions in "Using the library" — is
the entire surface a consumer needs. Everything else can change
without notice.

---

## Documentation

- [`docs/Architecture.md`](docs/Architecture.md) — the library's
  design: layers, components, and how they fit together.
- [`docs/Grammar.md`](docs/Grammar.md) — the complete language
  grammar.
- [`docs/FileStructure.md`](docs/FileStructure.md) — the directory
  layout.
- [`docs/notes/RegistryContractNote.md`](docs/notes/RegistryContractNote.md)
  — the format-library / engine boundary, with a full worked
  example of an engine-side registry.

---

## License

See [`LICENSE`](LICENSE).