# Registry contract: node kinds, categories, and the format-library / engine boundary

This note consolidates the design of the boundary between the
format library and the engine. The boundary is one data structure
— the `Registry` — and one question stands at its root:

> Should the format library or the engine define the node types
> and their categories?

The answer is not one or the other. It is **both, in different
senses**, and drawing that distinction cleanly is the whole point
of the design.

---

## 1. The two worlds

The format library and the engine are two different worlds. Each
knows things the other does not.

**The format library knows:**

- The grammar's rules — what productions are legal, what types
  exist, what a `Trigger` is.
- The vocabulary of node classification: the fact that nodes come
  in three kinds (`Value`, `Action`, `Trigger`), the fact that
  categories are free-form strings, the fact that phases are
  ordered.
- The rules that use the vocabulary: an `on` clause must target a
  trigger; an action must have an `on`; a value must be used
  somewhere.

**The engine knows:**

- What each specific node type *is* — its name, its arguments, its
  kind, its category.
- What each specific type *is* — the handles, the enums, the
  phases.
- What each node *does* at runtime.

The format library has the **rules**. The engine has the
**content**.

---

## 2. The registry is where they meet

The registry is the interface. It is populated by the engine and
read by Sema.

**The registry's shape is defined by the format library.**

The format library declares:

- `NodeKind` — the enum with three values.
- `NodeTypeInfo` — the struct with a name, kind, category, phase,
  argument list, and result type.
- `TypeId` — the tagged type reference.
- `PhaseInfo`, `EnumTypeInfo`, `EnumMemberInfo`, `HandleTypeInfo`
  — the other declarations.

These types live in the format library's headers. The engine
includes them to construct a registry.

**The registry's content is provided by the engine.**

The engine fills in the registry's arrays:

- For each node type: "this is called `MoveBody`, its kind is
  `Action`, its category is `Physics`, its phase is `Physics`, its
  arguments are `body: BodyRef`, `dx: float32`, `dy: float32`."
- For each enum: "`Key` has members `W=0, A=1, S=2, D=3`."
- For each handle: "`BodyRef` exists."
- For each phase: "the phases are `update`, `physics`, `render`,
  in that order."

The format library does not know any of this. The engine does.

**Sema reads the registry at compile time.** It uses the rules it
defines to check the content the engine provided. It produces a
`Graph`. It does not execute anything.

---

## 3. The split, in a table

| Concept                                                     | Format library defines | Engine defines            |
| ----------------------------------------------------------- | ---------------------- | ------------------------- |
| `NodeKind` enum                                             | The three values       | —                         |
| Which kind a node has                                       | —                      | The kind, in the registry |
| Node kind rules (`on` targeting, action subscription, etc.) | The rules              | —                         |
| Category (free-form string)                                 | The type               | The string value          |
| Phase ordering rules                                        | The concept            | The actual phases         |
| Node type names                                             | —                      | The names                 |
| Node arguments (names, types)                               | The shape              | The values                |
| Type kinds (`Primitive`, `Enum`, `Handle`)                  | The enum               | —                         |
| Which kind a type has                                       | —                      | The kind, in the registry |
| Type names                                                  | —                      | The names                 |
| Enum members and values                                     | —                      | The members, the values   |
| Handle type names                                           | —                      | The names                 |
| Sema's checks                                               | The checks             | —                         |
| Runtime behavior of nodes                                   | —                      | The behavior              |

**In one line:** the format library defines the vocabulary and the
rules; the engine populates the registry; Sema reads the registry
to apply the rules.

---

## 4. The kind: three values, defined once

The node kind is the sharpest example of the split.

The format library defines:

```cpp
enum class NodeKind : uint8_t
{
    Value,     // produces a value; no side effects
    Action,    // performs a side effect; must have at least one `on`
    Trigger,   // emits events; can be the target of an `on` clause
};
```

The engine assigns each node its kind when it writes the registry:

```cpp
static const NodeTypeInfo nodeTypes[] = {
    {"AddNode",     NodeKind::Value,   "Math",    0, ...},
    {"MoveBody",    NodeKind::Action,  "Physics", 1, ...},
    {"EveryFrame",  NodeKind::Trigger, "Flow",    0, ...},
};
```

The enum is defined once, in the format library. The engine uses
it. Sema uses it.

Sema reads the kind when it applies the rules:

```cpp
const NodeTypeInfo* info = lookupNodeType(registry, name);
if (info->kind != NodeKind::Trigger)
{
    // error: `on` target is not a trigger
}
```

The registry's `nodeTypes` array is a flat list; the engine does
not call a separate "register as action" function. The kind is a
field, and it is written inline. There is no third party.

---

## 5. The category: a string, opaque to the format library

The category is a different story.

The format library declares:

```cpp
struct NodeTypeInfo
{
    std::string_view name;
    NodeKind         kind;
    std::string_view category;  // free-form, like "Physics", "Flow", "Math"
    uint32_t         phase;
    ArenaSpan<NodeArgInfo> args;
    TypeId           resultType;
};
```

But the format library does not define what the categories *are*.
It stores whatever string the engine provides and passes it
through. It does not validate it. It does not use it in any Sema
check.

The engine chooses categories that match its own organization:

```cpp
{"AddNode",     NodeKind::Value,   "Math",    ...},
{"MoveBody",    NodeKind::Action,  "Physics", ...},
{"EveryFrame",  NodeKind::Trigger, "Flow",    ...},
{"OnCollision", NodeKind::Trigger, "Physics", ...},
```

The categories are for tools:

- The **editor** groups a node palette by category.
- The **documentation** lists nodes by category.
- A **linter** might report "this file has too many `Render`
  nodes."

The format library does not do any of that. It carries the string;
that is all.

If the engine chooses not to use categories at all, every node's
`category` can be empty. The format library does not care.

---

## 6. What Sema enforces, and what it does not

**Sema enforces, using the kind:**

1. An `on` clause target must be a Trigger node.
2. An Action node must have at least one `on` clause.
3. A Value node must be reachable from an Action node or from a
   resource field's default.
4. A node expression's arguments must match the declared argument
   list in count and type.
5. A resource field's type must resolve to a primitive, an enum,
   or a handle.
6. A resource field's default must be a constant of the field's
   type.

**Sema does not enforce, ignoring the category:**

1. What category a node belongs to.
2. How many nodes of a given category a file uses.
3. Whether a category name is spelled consistently.
4. Whether a node's category matches some external convention.

Category is a tool concern. Sema carries it and ignores it.

---

## 7. The direction of the dependency

An important consequence of this split is that **the format
library does not depend on any specific node type.**

The format library knows:

- There are three kinds.
- How to read a node type's kind.
- How to apply the kind-based rules.

It does not know:

- That `MoveBody` exists.
- That `OnCollision` exists.
- That `AddNode` exists.
- That `Physics` is a category.
- That `BodyRef` is a handle.

The format library is generic. It works with any engine, any node
set, any category scheme. The engine supplies the content at
registry-construction time; the format library does not need to be
updated when the engine adds a node.

This is why the format library can be a standalone library. If it
knew about specific node types, it would be coupled to one engine.
By keeping the registry as a runtime-filled interface, the format
library stays generic.

---

## 8. The three-layer picture

```
┌──────────────────────────────────────────────────────────┐
│                     The Engine                            │
│                                                           │
│   - Knows what each node does                             │
│   - Knows what each handle is                             │
│   - Knows the phases of its main loop                     │
│   - Builds the Registry at startup                        │
│   - Reads the Graph at runtime and executes it            │
│                                                           │
│                     ↑                                     │
│                     │ provides                            │
│                     │                                     │
├─────────────────────┼────────────────────────────────────┤
│                     │                                     │
│              The Registry                                 │
│                                                           │
│   - A data structure: names, kinds, categories,           │
│     arguments, types, phases                              │
│   - Defined by the format library                         │
│   - Populated by the engine                               │
│   - Read by Sema                                          │
│                                                           │
├─────────────────────┼────────────────────────────────────┤
│                     │ reads                               │
│                     │                                     │
│                     ↓                                     │
│                                                           │
│                The Format Library                         │
│                                                           │
│   - Defines the Registry's shape                          │
│   - Defines NodeKind, TypeId, and the rules that use them │
│   - Lexes, parses, formats, analyzes                      │
│   - Produces a Graph                                      │
│                                                           │
└──────────────────────────────────────────────────────────┘
```

The format library sits below the registry. The engine sits above
it. The registry is the interface between them.

The format library **defines the interface**. The engine **uses**
it.

---

## 9. The parallel with a compiler and a library

This split is the same as the relationship between a compiler and a
library.

**The compiler defines:**

- `int`, `float`, `char` — the primitive types.
- The rules: an `if` takes a boolean; a function call must match
  the declaration.

**The standard library defines:**

- `printf`, `malloc`, `strlen` — the specific functions.
- Their signatures: what arguments they take, what they return.

**The compiler does not know what `printf` does.** It knows `printf`
is a function that takes a `const char*` and returns an `int`. It
checks that callers pass the right arguments. It does not know that
`printf` writes to stdout.

The compiler is the referee. The library is the declarer. They
meet at the declaration.

The format library is the compiler. The engine's node set is the
standard library. The registry is the declaration.

---

## 10. What this means for the implementation

The implications for Sema and the CLI:

1. **`NodeKind` is defined once.** In `sema/include/sema/NodeKind.hpp`.
   Three values. No more.

2. **The registry is a data structure.** A struct holding spans of
   `NodeTypeInfo`, `EnumTypeInfo`, `HandleTypeInfo`, and
   `PhaseInfo`. Built by the engine. Read by Sema.

3. **Sema has no hard-coded node types.** It looks up every node
   type by name in the registry. It never references `MoveBody` or
   `AddNode` directly.

4. **Sema enforces kind-based rules.** The six rules in §6 depend
   on `NodeKind` or on a resolved `TypeId`. They are implemented
   in `sema/src/sema/TypeChecker.cpp`,
   `sema/src/sema/DeadCodeChecker.cpp`, and the graph builder.

5. **The category is carried but not used.**
   `NodeTypeInfo::category` is stored in the registry and passed
   through to the graph builder's diagnostics. Sema never reads
   it.

6. **The graph does not carry the category.** Since the category
   is tool metadata, the graph does not need it at runtime. The
   engine can look it up from the registry by `type_id` if it
   wants it. The `Graph` structure stores node type IDs, not
   categories.

7. **The registry's contents are the engine's responsibility.**
   The engine must build the registry before compiling. It must
   keep the arrays alive during the compile. Errors in the
   registry — duplicate names, missing members, wrong values —
   are the engine's bugs, not the user's.

8. **`computeRegistryFingerprint` produces a stable hash.** Two
   registries with the same content produce the same fingerprint.
   Change one entry and the fingerprint changes. This is what
   lets a `.lucgraph` refuse to load against the wrong registry.

---

## 11. The questions this settles

Everything from the design conversation, in one list:

| Question                                                                | Answer                                |
| ----------------------------------------------------------------------- | ------------------------------------- |
| Who defines the `NodeKind` enum?                                        | The format library.                   |
| Who defines which kind each node has?                                   | The engine, in the registry.          |
| Who defines the category strings?                                       | The engine.                           |
| Does Sema use the category?                                             | No.                                   |
| Does the graph carry the category?                                      | No. Tools query the registry.         |
| What Sema checks about kind                                             | The six rules in §6.                  |
| What Sema checks about category                                         | Nothing.                              |
| Can the format library reference any specific node type?                | No. It is generic.                    |
| Can the format library reference any specific category?                 | No. Categories are opaque strings.    |
| Can the engine add a new node type without changing the format library? | Yes.                                  |
| Can the engine add a new category without changing the format library?  | Yes.                                  |
| Can the engine add a new phase without changing the format library?     | Yes.                                  |
| Can the engine change an enum's backing type?                           | Yes. The script layer never sees it.  |
| Does the format library know what a `BodyRef` is?                       | No. Just the name.                    |
| Does the format library know what `MoveBody` does?                      | No. Just the arguments.               |
| Where does the runtime implementation live?                             | The engine. Never the format library. |

---

## 12. The rule, stated once

**The format library defines the vocabulary — the enums, the
shapes — and the rules that use it. The engine defines the
content — the names, the kinds, the values, the behaviors. The
registry is where they meet. Sema reads the registry to apply the
format library's rules to the engine's content. Nothing flows the
other way.**

**A note on the word "category."** Earlier design notes used
"category" in two different senses. This note distinguishes them
once, finally:

- **The kind category**: `Value`, `Action`, `Trigger`. Defined by
  the format library. Used by Sema.
- **The organizational category**: `"Physics"`, `"Flow"`, `"Math"`.
  Defined by the engine. Used by tools.

In this note, "kind" always means the first, and "category" always
means the second. The two are orthogonal.

---

## 13. The headers that define the contract

The registry's shape lives in these files. The engine includes
them; the format library owns them.

- `sema/include/sema/NodeKind.hpp` — the three-value enum and its
  name function.
- `sema/include/sema/Registry.hpp` — the registry structs
  (`NodeTypeInfo`, `NodeArgInfo`, `EnumTypeInfo`, `EnumMemberInfo`,
  `HandleTypeInfo`, `PhaseInfo`, `Registry`) and the
  `computeRegistryFingerprint` declaration.
- `sema/include/sema/TypeId.hpp` — the tagged type reference.
- `sema/include/sema/Literal.hpp` — the compile-time value type.
- `sema/include/sema/Graph.hpp` — the graph structs (`Graph`,
  `NodeInstance`, `Arg`, `Resource`, `ResourceField`).
- `sema/include/sema/Sema.hpp` — the public API
  (`CompileOptions`, `CompileResult`, `compile`,
  `compileModule`).
- `core/include/core/diagnostics/DiagCode.hpp` — the diagnostic
  codes Sema uses (the `Name_*`, `Type_*`, `Trigger_*`, and
  `Attr_*` bands).

Everything else is internal.

---

## 14. A complete engine-side registry

Sections 1–13 explain the boundary. This section shows what the
engine team writes to honor it. The sample is a compilable
registry for a small engine — enough to compile the sample
programs in the README and to load the `.lucgraph` files they
produce.

### 14.1 The header

```cpp
// my_engine/include/my_engine/Registry.hpp

#pragma once

#include "sema/Registry.hpp"

namespace my_engine
{

    /// The engine's registry, built once at process start.
    /// The returned reference is valid for the process's lifetime.
    const lucid::sema::Registry& registry();

} // namespace my_engine
```

### 14.2 The definition

```cpp
// my_engine/src/my_engine/Registry.cpp

#include "my_engine/Registry.hpp"

namespace my_engine
{

    const lucid::sema::Registry& registry()
    {
        using namespace lucid::sema;

        // ─── Phases ───────────────────────────────────────────────────────
        //
        // The order of this array is the engine's main-loop order.
        // Action nodes are sorted by their `phase` field at compile
        // time; the resulting `phase_order` in the graph is the
        // flattened result. Do not reorder without regenerating
        // every .lucgraph.
        static const PhaseInfo phases[] = {
            {"update"},    // index 0
            {"physics"},   // index 1
            {"render"},    // index 2
        };

        // ─── Enums ────────────────────────────────────────────────────────
        //
        // Enum members carry host-assigned integer values. The script
        // writes `Key.W`; Sema resolves it to 0 and stores the integer
        // in the graph. The engine never sees the member's name at
        // runtime — only the integer.
        static const EnumMemberInfo keyMembers[] = {
            {"W", 0},
            {"A", 1},
            {"S", 2},
            {"D", 3},
        };
        static const EnumTypeInfo enums[] = {
            {"Key", ArenaSpan<EnumMemberInfo>(keyMembers, 4)},
        };

        // ─── Handles ──────────────────────────────────────────────────────
        //
        // Opaque reference types. The format library never inspects
        // what a handle points at; the engine does. A handle is a
        // first-class value: it can be a node argument, a resource
        // field, or a Value node's result.
        static const HandleTypeInfo handles[] = {
            {"BodyRef"},
            {"TextureRef"},
        };

        // ─── Node argument lists ──────────────────────────────────────────
        //
        // Each node type declares its arguments positionally. Sema
        // checks every call site against this list: the count and the
        // type of each argument.
        //
        // The `NodeArgInfo::name` field is for diagnostics only. A
        // call site writes `MoveBody(body, 1.0, 0.0)`, not
        // `MoveBody(body = body, dx = 1.0, dy = 0.0)`.

        static const NodeArgInfo float32Args[] = {
            {"value", TypeId::primitive("float32")},
        };
        static const NodeArgInfo int32Args[] = {
            {"value", TypeId::primitive("int32")},
        };
        static const NodeArgInfo addArgs[] = {
            {"a", TypeId::primitive("float32")},
            {"b", TypeId::primitive("float32")},
        };
        static const NodeArgInfo moveBodyArgs[] = {
            {"body", TypeId::handle("BodyRef")},
            {"dx",   TypeId::primitive("float32")},
            {"dy",   TypeId::primitive("float32")},
        };
        static const NodeArgInfo printArgs[] = {
            {"value", TypeId::primitive("float32")},
        };
        static const NodeArgInfo damageArgs[] = {
            {"body",   TypeId::handle("BodyRef")},
            {"amount", TypeId::primitive("int32")},
        };

        // ─── Node types ───────────────────────────────────────────────────
        //
        // This array's order is the order of NodeId values in every
        // .lucgraph file. Do not reorder. Adding a node at the end is
        // safe as long as every .lucgraph is regenerated; adding one
        // in the middle renumbers every node after it.
        //
        // The `kind` field is the sharpest example of the split. The
        // engine chooses the kind when it writes this array. Sema
        // reads it to enforce the kind-based rules (on-target must be
        // a Trigger, Action must have an `on`, Value must be used).
        //
        // The `category` field is a free-form string the format
        // library carries but never reads. It exists for tools: an
        // editor's node palette, a documentation index.
        //
        // The `phase` field is meaningful only for Action nodes. It
        // indexes into `phases` above. A Value or Trigger node may
        // leave it at 0; Sema ignores it.

        static const NodeTypeInfo nodeTypes[] = {
            // ─── Value nodes ──────────────────────────────────────────────
            {
                "Float32Node",
                NodeKind::Value,
                "Math",                       // category
                0,                            // phase (unused)
                ArenaSpan<NodeArgInfo>(float32Args, 1),
                TypeId::primitive("float32"), // result type
            },
            {
                "Int32Node",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(int32Args, 1),
                TypeId::primitive("int32"),
            },
            {
                "AddNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(addArgs, 2),
                TypeId::primitive("float32"),
            },

            // ─── Action nodes ─────────────────────────────────────────────
            {
                "MoveBody",
                NodeKind::Action,
                "Physics",
                1,                            // phase index: "physics"
                ArenaSpan<NodeArgInfo>(moveBodyArgs, 3),
                TypeId{},                     // no result
            },
            {
                "PrintNode",
                NodeKind::Action,
                "Debug",
                0,                            // phase index: "update"
                ArenaSpan<NodeArgInfo>(printArgs, 1),
                TypeId{},
            },
            {
                "Damage",
                NodeKind::Action,
                "Physics",
                1,                            // phase index: "physics"
                ArenaSpan<NodeArgInfo>(damageArgs, 2),
                TypeId{},
            },

            // ─── Trigger nodes ────────────────────────────────────────────
            {
                "EveryFrame",
                NodeKind::Trigger,
                "Flow",
                0,
                ArenaSpan<NodeArgInfo>{},
                TypeId{},
            },
        };

        // ─── The Registry ─────────────────────────────────────────────────

        static const Registry reg = []
        {
            Registry r;
            r.phases    = ArenaSpan<PhaseInfo>(phases, 3);
            r.enums     = ArenaSpan<EnumTypeInfo>(enums, 1);
            r.handles   = ArenaSpan<HandleTypeInfo>(handles, 2);
            r.nodeTypes = ArenaSpan<NodeTypeInfo>(nodeTypes, 7);
            return r;
        }();

        return reg;
    }

} // namespace my_engine
```

### 14.3 Compile and serialize

```cpp
#include "my_engine/Registry.hpp"
#include "sema/Sema.hpp"
#include "serialization/Serializer.hpp"

void compile_and_write(const std::string& source,
                       const std::string& outputPath)
{
    auto result = lucid::sema::compile(
        source, "player.lucid", my_engine::registry());

    if (!result.ok)
    {
        for (const auto& d : result.diagnostics)
        {
            std::cerr << lucid::diag::severityName(d.severity) << ": "
                      << d.message << "\n";
        }
        return;
    }

    const std::vector<uint8_t> bytes =
        lucid::serialization::serialize(*result.graph);

    writeFile(outputPath, bytes);
}
```

### 14.4 Load and deserialize

```cpp
#include "my_engine/Registry.hpp"
#include "serialization/Deserializer.hpp"

std::unique_ptr<lucid::sema::Graph>
load_graph(const std::string& path)
{
    const std::vector<uint8_t> bytes = readFile(path);

    auto dr = lucid::serialization::deserialize(
        lucid::serialization::ByteSpan(bytes),
        my_engine::registry());

    if (!dr.ok)
    {
        // The file was compiled against a different registry, or is
        // corrupt. Report and fail the load.
        for (const auto& d : dr.diagnostics)
        {
            std::cerr << lucid::diag::severityName(d.severity) << ": "
                      << d.message << "\n";
        }
        return nullptr;
    }

    return std::move(dr.graph);
}
```

The registry is passed to both. It is the same object at compile
time and at load time. The fingerprint the library computes from
it is written into the file on `serialize` and checked against it
on `deserialize`. If the engine's registry changed between the two
calls, the check fails and the load is refused — which is what you
want, because the `.lucgraph` would otherwise misbind every
`NodeId` it contains.

### 14.5 Runtime dispatch

The registry tells the format library *what* a node type is. The
engine tells itself *how* it runs. The link between them is the
index of the node type in `nodeTypes`.

```cpp
#include "my_engine/Registry.hpp"
#include "sema/Graph.hpp"

namespace my_engine
{

    struct ExecutionContext
    {
        // One result per node, indexed by NodeIndex.
        std::vector<float> results;

        // The engine's own handle table. A handle is an opaque token
        // in the graph; the engine maps it back to a real object.
        std::unordered_map<uint32_t, Body*> bodies;

        double deltaTime = 0.0;
        // ...
    };

    // An executor reads one node's arguments from the graph, does
    // the work, and — for a Value node — stores the result in
    // `ctx.results[i]`. A NodeIndex names the node being executed;
    // the graph is where the node's record lives.
    using NodeExecutor = void (*)(ExecutionContext&,
                                  lucid::sema::NodeIndex,
                                  const lucid::sema::Graph&);

    // One executor per entry in registry().nodeTypes, in the same
    // order. NodeInstance::type_id indexes this array.
    static const NodeExecutor executors[] = {
        // ─── 0: Float32Node ───────────────────────────────────────────────
        [](ExecutionContext& ctx, lucid::sema::NodeIndex i,
           const lucid::sema::Graph& g) {
            const auto args = g.argsOf(g.nodes[i]);
            ctx.results[i] = args[0].literal.f;
        },
        // ─── 1: Int32Node ─────────────────────────────────────────────────
        [](ExecutionContext& ctx, lucid::sema::NodeIndex i,
           const lucid::sema::Graph& g) {
            const auto args = g.argsOf(g.nodes[i]);
            ctx.results[i] = static_cast<float>(args[0].literal.i);
        },
        // ─── 2: AddNode ───────────────────────────────────────────────────
        [](ExecutionContext& ctx, lucid::sema::NodeIndex i,
           const lucid::sema::Graph& g) {
            const auto args = g.argsOf(g.nodes[i]);
            const float a = ctx.results[args[0].node_ref];
            const float b = ctx.results[args[1].node_ref];
            ctx.results[i] = a + b;
        },
        // ─── 3: MoveBody ──────────────────────────────────────────────────
        [](ExecutionContext& ctx, lucid::sema::NodeIndex i,
           const lucid::sema::Graph& g) {
            const auto args = g.argsOf(g.nodes[i]);
            Body* body = resolveBody(ctx, args[0]);
            const float dx = args[1].literal.f;
            const float dy = args[2].literal.f;
            physics_move(body, dx, dy, ctx.deltaTime);
        },
        // ─── 4: PrintNode ─────────────────────────────────────────────────
        [](ExecutionContext& ctx, lucid::sema::NodeIndex i,
           const lucid::sema::Graph& g) {
            const auto args = g.argsOf(g.nodes[i]);
            std::cout << args[0].literal.f << "\n";
        },
        // ─── 5: Damage ────────────────────────────────────────────────────
        [](ExecutionContext& ctx, lucid::sema::NodeIndex i,
           const lucid::sema::Graph& g) {
            const auto args = g.argsOf(g.nodes[i]);
            Body* body = resolveBody(ctx, args[0]);
            const int32_t amount =
                static_cast<int32_t>(args[1].literal.i);
            apply_damage(body, amount);
        },
        // ─── 6: EveryFrame ────────────────────────────────────────────────
        [](ExecutionContext&, lucid::sema::NodeIndex,
           const lucid::sema::Graph&) {
            // Triggers do not execute. They fire subscribers, which
            // the engine's scheduler handles separately.
        },
    };

    /// One tick of the engine's main loop.
    void tick(ExecutionContext& ctx, const lucid::sema::Graph& g)
    {
        // 1. Fire triggers. The engine's scheduler decides which
        //    trigger nodes fire this frame and dispatches each
        //    subscriber's action.
        //
        //    (The engine's own logic; not shown here.)

        // 2. Run action nodes in phase order.
        for (lucid::sema::NodeIndex i : g.phase_order)
        {
            const auto& node = g.nodes[i];
            executors[node.type_id](ctx, i, g);
        }

        // 3. Value nodes are evaluated on demand: an action reads
        //    ctx.results[value_index], and the engine ensures that
        //    value has already been computed. The engine's scheduler
        //    uses g.value_order for this.
    }

} // namespace my_engine
```

The two arrays — `registry().nodeTypes` and `executors` — are the
engine's private contract with itself. A new node type is added to
both, at the same index, and the fingerprint changes. Nothing else
about the format library changes; the library does not know the
node's name, its behavior, or its category, and does not need to.

#### 14.5.1 Gating triggers

The dispatch above runs actions. It does not describe how triggers
fire, because triggers do not "run" — they fire, and the engine's
scheduler decides when.

The scheduler needs one additional piece of engine-side metadata:
for each node type, whether it is a *source trigger* (the engine
fires it directly) or a *gating trigger* (it fires only when a
parent fires and a condition holds). The library's `Registry`
does not carry this distinction, because the library does not give
`on` a meaning. The engine does.

```cpp
enum class TriggerMode : uint8_t
{
    NotATrigger,   // Value or Action; fires nothing
    Source,        // fires because the engine says so
    Gating,        // fires when a parent fires and
                   // the condition holds
};
```

`subscribers` is the graph edge list: a node's subscribers are the
nodes that depend on it. The engine's scheduler consults the
trigger mode for each node type and applies the condition test for
`Gating` triggers before dispatching their subscribers. The library
stores the graph shape; the engine stores the behavior.

This split is deliberate. A trigger's flow-control meaning is not a
property of the grammar, and not a property of the format library.
It is a property of the engine's runtime model. The registry only
names the node type and its kind. The scheduler decides how that
kind participates in the event graph.

### 14.6 What the engine team must remember

Five rules that follow from the split:

1. **Keep the arrays alive.** The `Registry` holds spans into
   `static const` arrays. If any of them lives on the stack or is
   freed, every compile and every load is undefined behavior.
   `static const` at namespace scope, as above, is the safe form.

2. **Do not reorder `nodeTypes`.** The order is the wire format's
   `NodeId` space. Adding a node at the end is safe (after
   regenerating every `.lucgraph`); inserting one in the middle
   renumbers every node after it. The fingerprint changes either
   way, so the loader will refuse stale files — but the engine's
   `executors` array must be reordered too, or the executor for
   index `i` will run the wrong node type.

3. **Do not reorder `phases`.** Phase indices are stored in the
   graph's `NodeInstance::phase` field. A reorder changes the
   fingerprint and refuses every stale file, but it also changes
   which phase an action runs in for a fresh compile. Do it
   deliberately, with a changelog entry.

4. **Do not reorder an enum's members.** Enum member values are
   integers the script writes by name and Sema resolves to the
   integer. A reorder changes the fingerprint, refuses stale files,
   and — more subtly — changes the meaning of any script that is
   *recompiled* against the reordered registry. The script's
   source still says `Key.W`; the integer behind `Key.W` is
   different.

5. **Bump the version byte in `computeRegistryFingerprint` only
   when the algorithm changes.** The fingerprint's version byte is
   the format library's, not the engine's. The engine changes the
   registry by editing its content (the arrays); the fingerprint
   changes automatically because it hashes that content. Editing
   the version byte is for the format library team when the
   hashing algorithm itself changes.
