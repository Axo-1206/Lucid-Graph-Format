# Consolidated: Node kinds, node categories, and the format library / engine boundary

Let me consolidate the entire conversation into a single design statement, because there is one question at the root of everything we have discussed, and it deserves a clean answer.

---

## 1. The question, precisely

> **"Should Sema or the engine define the node types and categories?"**

The answer is not "Sema" or "the engine." It is **both, in different senses**, and the boundary between the two is the whole point of the design.

---

## 2. The two worlds

The format library and the engine are two different worlds. Each one knows things the other does not.

**The format library knows:**

- The grammar's rules (what productions are legal, what types exist, what `Event` means).
- The **vocabulary** of node classification: the fact that nodes come in three kinds (`Value`, `Action`, `Trigger`), the fact that categories are strings, the fact that phases are ordered.
- The **rules that use the vocabulary**: an `on` clause must target a trigger; an action must have an `on`; a value must be used; a composite's `Event` output must resolve to a trigger.

**The engine knows:**

- What each specific node type *is* — its name, its ports, its kind, its category.
- What each specific type *is* — the handles, the enums, the phases.
- What each node *does* at runtime.

The format library has the **rules**; the engine has the **content**.

---

## 3. The registry is where they meet

The registry is the interface. It is populated by the engine and read by Sema.

**The registry's *shape* is defined by the format library.**

The format library declares:

- `NodeKind` — the enum with three values.
- `NodeTypeInfo` — the struct with a name, kind, category, phase, and ports.
- `TypeId` — the tagged type reference.
- `PhaseInfo`, `EnumTypeInfo`, `HandleTypeInfo` — the other declarations.

These types live in the format library's headers. The engine includes them to construct a registry.

**The registry's *content* is provided by the engine.**

The engine fills in the registry's arrays:

- For each node type: "this is called `MoveBody`, its kind is `Action`, its category is `Physics`, its phase is `Physics`, its ports are `body: BodyRef` and `target: Vec2`."
- For each enum: "`Key` has members `W=0, A=1, S=2, D=3`."
- For each handle: "`BodyRef` exists."
- For each phase: "the phases are `Input`, `Update`, `Physics`, `Render`, in that order."

The format library does not know *any* of this. The engine does.

**Sema reads the registry at compile time.** It uses the rules it defines to check the content the engine provided. It produces a graph. It does not execute anything.

---

## 4. The split, in a table

| Concept                                                     | Format library defines | Engine defines                     |
| ----------------------------------------------------------- | ---------------------- | ---------------------------------- |
| `NodeKind` enum                                             | The three values       | —                                  |
| Which kind a node has                                       | —                      | The kind (via which register call) |
| Node kind rules (`on` targeting, action subscription, etc.) | The rules              | —                                  |
| Category (free-form string)                                 | The type               | The string value                   |
| Phase ordering rules                                        | The concept            | The actual phases                  |
| Node type names                                             | —                      | The names                          |
| Node ports (names, types)                                   | The shape              | The values                         |
| Type kinds (`Primitive`, `Enum`, `Handle`, `Event`)         | The enum               | —                                  |
| Which kind a type has                                       | —                      | The kind (via which register call) |
| Type names                                                  | —                      | The names                          |
| Enum members and values                                     | —                      | The members, the values            |
| Handle type names                                           | —                      | The names                          |
| Sema's checks                                               | The checks             | —                                  |
| Runtime behavior of nodes                                   | —                      | The behavior                       |

**Rule in one line:** the format library defines the vocabulary and the rules; the engine populates the registry; Sema reads the registry to apply the rules.

---

## 5. The kind: three values, defined once

The node kind is the sharpest example of the split.

The format library defines:

```cpp
// In the format library's public header.
enum class NodeKind : uint8_t {
    Value,     // produces a value; no side effects
    Action,    // performs a side effect; must have `on`
    Trigger,   // emits events; can be subscribed to with `on`
};
```

The engine assigns each node its kind when it registers:

```cpp
registry.add_value_node  ("AddNode",     "Math");
registry.add_action_node ("MoveBody",    "Physics", Phase::Physics);
registry.add_trigger_node("OnCollision", "Physics");
```

The three register functions are the format library's API. Each one attaches a different `NodeKind` to the node being registered. The engine chooses which function to call; the format library knows what each choice means.

Sema reads the kind when it applies the rules:

```cpp
const NodeTypeInfo* decl = lookupNodeType(node.type_name);
if (decl->kind != NodeKind::Trigger) {
    // error: `on` target is not a trigger
}
```

The enum is defined once, in the format library. The engine uses it. Sema uses it. There is no third party.

---

## 6. The category: a string, opaque to the format library

The category is a different story.

The format library declares:

```cpp
struct NodeTypeInfo {
    // ...
    std::string_view category;   // free-form, like "Physics", "Flow", "Math"
    // ...
};
```

But the format library does not define what the categories *are*. It stores whatever string the engine provides and passes it through. It does not validate it. It does not use it in any Sema check.

The engine chooses categories that match its own organization:

```cpp
registry.add_value_node  ("AddNode",     "Math");
registry.add_action_node ("MoveBody",    "Physics", Phase::Physics);
registry.add_trigger_node("EveryFrame",  "Flow");
registry.add_trigger_node("OnCollision", "Physics");
```

The categories are for tools:

- **The editor** groups a node palette by category.
- **The documentation** lists nodes by category.
- **A linter** might report "this file has too many `Render` nodes."

The format library does not do any of that. It carries the string; that is all.

**If the engine chooses to not use categories at all**, every node's `category` can be empty. The format library does not care.

---

## 7. What Sema enforces, and what it does not

**Sema enforces (using the kind):**

1. An `on` clause target must be a trigger node or an `Event`-typed composite output.
2. An action node must have at least one `on` clause.
3. A composite's `Event`-typed output's right-hand side must resolve to a trigger node.
4. A value node must be reachable from an action node or from an output binding.

**Sema does not enforce (category is not used):**

1. What category a node belongs to.
2. How many nodes of a given category a file uses.
3. Whether a category name is spelled consistently.
4. Whether a node's category matches some external convention.

Category is a tool concern. Sema carries it and ignores it.

---

## 8. The direction of the dependency

An important consequence of this split is that **the format library does not depend on any specific node type.**

The format library knows:

- There are three kinds.
- How to read a node type's kind.
- How to apply the kind-based rules.

It does **not** know:

- That `MoveBody` exists.
- That `OnCollision` exists.
- That `AddNode` exists.
- That `Physics` is a category.
- That `BodyRef` is a handle.

The format library is generic. It works with any engine, any node set, any category scheme. The engine supplies the content at registration time; the format library does not need to be updated when the engine adds a node.

**This is why the format library can be a standalone library** (`Architecture.md` §1). If it knew about specific node types, it would be coupled to one engine. By keeping the registry as a runtime-filled interface, the format library stays generic.

---

## 9. The three-layer picture

Let me draw the whole thing so the boundary is unambiguous.

```
┌──────────────────────────────────────────────────────────┐
│                     The Engine                            │
│                                                           │
│   - Knows what each node does                            │
│   - Knows what each handle is                            │
│   - Knows the phases of its main loop                    │
│   - Registers everything into a Registry at startup      │
│   - Reads the Graph at runtime and executes it           │
│                                                           │
│                     ↑                                     │
│                     │ provides                            │
│                     │                                     │
├─────────────────────┼────────────────────────────────────┤
│                     │                                     │
│              The Registry                                 │
│                                                           │
│   - A data structure: names, kinds, categories,           │
│     ports, types, phases                                 │
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

The format library sits below the registry. The engine sits above it. The registry is the interface between them.

The format library **defines the interface**. The engine **uses** it.

---

## 10. The parallel with a compiler and a library

This split is exactly the relationship between a compiler and a library.

**The compiler defines:**

- `int`, `float`, `char` — the primitive types.
- The rules: an `if` takes a boolean; a function call must match the declaration.

**The standard library defines:**

- `printf`, `malloc`, `strlen` — the specific functions.
- Their signatures: what arguments they take, what they return.

**The compiler does not know what `printf` does.** It knows `printf` is a function that takes a `const char*` and returns an `int`. It checks that callers pass the right arguments. It does not know that `printf` writes to stdout.

The compiler is the referee. The library is the declarer. They meet at the declaration.

The format library is the compiler. The engine's node set is the standard library. The registry is the declaration.

---

## 11. What this means for Phase 7

The implications for Sema's implementation:

**1. `NodeKind` is defined once.** In `sema/include/sema/Registry.hpp` (or a nearby header). Three values. No more.

**2. The registry is a data structure.** A struct holding spans of `NodeTypeInfo`, `EnumTypeInfo`, `HandleTypeInfo`, `PhaseInfo`. Filled by the engine. Read by Sema.

**3. Sema has no hard-coded node types.** It looks up every node type by name in the registry. It never references `MoveBody` or `AddNode` directly.

**4. Sema enforces kind-based rules.** Six rules depend on `NodeKind`. They are implemented in `TypeChecker.cpp` and `DeadCodeChecker.cpp`.

**5. The category is carried but not used.** `NodeTypeInfo::category` is stored in the registry and passed through to the graph (if the graph stores it; see below). Sema never reads it.

**6. The graph may or may not carry the category.** Since the category is tool metadata, the graph does not need it at runtime. The engine can look it up from the registry by `type_id` if it wants it. **Recommendation: the graph does not store the category.** The engine's editor and tools query the registry directly.

**7. The registry's contents are the engine's responsibility.** The engine must populate the registry before compiling. It must keep the data alive during the compile. Errors in the registry (duplicate names, missing members, wrong values) are the engine's bugs, not the user's.

---

## 12. The questions that are now settled

Everything from the conversation, in one list:

| Question                                                                | Answer                                  |
| ----------------------------------------------------------------------- | --------------------------------------- |
| Who defines the `NodeKind` enum?                                        | The format library.                     |
| Who defines which kind each node has?                                   | The engine, when it registers the node. |
| Who defines the category strings?                                       | The engine.                             |
| Does Sema use the category?                                             | No.                                     |
| Does the graph carry the category?                                      | No. Tools query the registry.           |
| What Sema checks about kind:                                            | Six rules (§7).                         |
| What Sema checks about category:                                        | Nothing.                                |
| Can the format library reference any specific node type?                | No. It is generic.                      |
| Can the format library reference any specific category?                 | No. Categories are opaque strings.      |
| Can the engine add a new node type without changing the format library? | Yes.                                    |
| Can the engine add a new category without changing the format library?  | Yes.                                    |
| Can the engine add a new phase without changing the format library?     | Yes.                                    |
| Can the engine change an enum's backing type?                           | Yes. The script layer never sees it.    |
| Does the format library know what a `BodyRef` is?                       | No. Just the name.                      |
| Does the format library know what `MoveBody` does?                      | No. Just the ports.                     |
| Where does the runtime implementation live?                             | The engine. Never the format library.   |

---

## 13. The rule, stated once

**The format library defines the vocabulary (the enums, the shapes) and the rules that use it. The engine defines the content (the names, the kinds, the values, the behaviors). The registry is where they meet. Sema reads the registry to apply the format library's rules to the engine's content. Nothing flows the other way.**

**A note on the word "category."** In the previous messages, "category" was used in two different senses. Let me disambiguate once, finally:

- **The kind category**: `Value`, `Action`, `Trigger`. Defined by the format library. Used by Sema.
- **The organizational category**: `"Physics"`, `"Flow"`, `"Math"`. Defined by the engine. Used by tools.

In this consolidated design, "kind" always means the first, and "category" always means the second. The two are orthogonal.

---

## 14. What comes next

The design is now settled. Phase 7 begins with the type definitions:

- `sema/include/sema/NodeKind.hpp` — the three-value enum.
- `sema/include/sema/Registry.hpp` — the registry structs (`NodeTypeInfo`, `NodePortInfo`, `EnumTypeInfo`, `EnumMemberInfo`, `HandleTypeInfo`, `PhaseInfo`, `Registry`).
- `sema/include/sema/TypeId.hpp` — the tagged type reference.
- `sema/include/sema/Literal.hpp` — the literal value type.
- `sema/include/sema/Graph.hpp` — the graph structs (`Graph`, `NodeInstance`, `Arg`, `Resource`, `ResourceField`).
- `sema/include/sema/Sema.hpp` — the public API (`CompileOptions`, `CompileResult`, `compile`, `compileModule`).
- `sema/include/sema/Diagnostics.hpp` — Sema-specific diagnostic codes (extending the existing `DiagCode`).

Each header is small and self-contained. The tests for Phase 7, Step 7.1 cover:

- `NodeKind` values and names.
- `TypeId`'s kind discrimination.
- `Literal`'s kind discrimination and union access.
- `Registry` construction and default values.
- `Graph` construction and slice accessors.
- `computeRegistryFingerprint` on a small registry.

After Step 7.1, the types are in place. Step 7.2 begins the symbol collection pass.

Reply with any changes to this consolidated design, or confirm it and we proceed to Phase 7, Step 7.1.