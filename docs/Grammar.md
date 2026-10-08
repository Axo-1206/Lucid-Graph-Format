# Lucid Graph Format — Grammar

The complete grammar. This supersedes the previous grammar. Every production is here; nothing is left implicit. Open questions and known gaps are collected at the end.

---

## Notation

```
'terminal'      a literal token
NAME            a non-terminal production
[ x ]           optional
{ x }           zero or more
x | y           alternative
( x )           grouping
x ,             trailing comma shown explicitly where allowed
```

Whitespace and comments are ignored between tokens.

---

## 1. Lexical grammar

### 1.1 Source

Source files are UTF-8. A file is a sequence of tokens.

### 1.2 Keywords

```
import   from
enum     resource
node     on
```

`@export` is lexed as an attribute token (see §1.6).

### 1.3 Identifiers

```
IDENTIFIER ::= LETTER { LETTER | DIGIT | '_' }
LETTER     ::= 'a'..'z' | 'A'..'Z' | '_'
DIGIT      ::= '0'..'9'
```

Identifiers are case-sensitive. Keywords may not be used as identifiers.

### 1.4 Literals

```
INT_LIT    ::= DIGIT+ | '0x' HEX+ | '0b' BIN+ | '0o' OCT+
FLOAT_LIT  ::= DIGIT+ '.' DIGIT+ [ ('e'|'E') ['+'|'-'] DIGIT+ ]
STRING_LIT ::= '"' { STRING_CHAR } '"'
CHAR_LIT   ::= '\'' ( CHAR_CHAR | ESCAPE ) '\''
BOOL_LIT   ::= 'true' | 'false'
NIL_LIT    ::= 'nil'

HEX        ::= DIGIT | 'a'..'f' | 'A'..'F'
BIN        ::= '0' | '1'
OCT        ::= '0'..'7'

STRING_CHAR ::= ANY_CHAR_EXCEPT('"', '\', NEWLINE) | ESCAPE
CHAR_CHAR   ::= ANY_CHAR_EXCEPT('\'', '\', NEWLINE)
ESCAPE      ::= '\' ( 'n' | 't' | 'r' | '\' | '\'' | '"' | '0' )
```

A `FLOAT_LIT` requires at least one digit on each side of the `.`. `1.` and `.5` are not valid floats; write `1.0` and `0.5`.

### 1.5 Comments

```
line_comment  ::= '--' { ANY_CHAR_EXCEPT_NEWLINE } NEWLINE
block_comment ::= '/-' { ANY_CHAR | block_comment } '-/'    -- nestable
```

Comments are ignored. There are no doc comments.

### 1.6 Punctuation and attributes

```
( ) { } [ ] , . : :: = @
```

`@` is the attribute sigil. It is always followed by an identifier and never appears alone. The only attribute the format recognizes is `@export`.

`@export` is lexed as two tokens: `@` and the identifier `export`. The parser combines them; Sema interprets `export` as the visibility attribute.

`[` and `]` are **reserved**. No production in the current grammar uses them; they are lexed so that a future array or index syntax does not require a lexer change. A `[` or `]` in source is a syntax error at the parser, not the lexer.

`:` is the resource-field separator (`name: type`). `::` is the module qualifier (`module::Name`). The two are distinct tokens; there is no ambiguity between them.

### 1.7 Whitespace

Whitespace is any sequence of spaces, tabs, carriage returns, and newlines. It separates tokens and is otherwise ignored.

---

## 2. Syntactic grammar

### 2.1 Program

```
program     ::= { top_decl }

top_decl    ::= import_decl
              | enum_decl
              | resource_decl
              | node_decl
```

The top level contains only declarations. There are no top-level statements. Nothing runs at load time.

### 2.2 Imports

```
import_decl ::= attribute_list 'import' module_path [ 'as' IDENTIFIER ]
module_path ::= IDENTIFIER { '.' IDENTIFIER }
```

`import core.keys` loads `core/keys.lucid` and binds it to the local name `keys`.
`import core.keys as k` binds it to `k`.

An `import_decl` may appear anywhere among the top-level declarations of a module.

**Import model.** An import binds the module's exported declarations into the importing module's scope. An exported `enum`, `resource`, or `node` is referenced by its bare name after import. When two imports would collide, the author uses `as` to bind a distinct local name, and reaches the declaration through that alias.

An alias is used as a **qualifier** with `::`. For example, if a module is imported as `k`, a type from it is written `k::Key`, and a node type from it is written `k::Health`. The dot is reserved for field access; it never names a module.

**Depth.** A qualified name has at most one qualifier. `k::Key` is valid; `a::b::Key` is not. If a module re-exports a type from a third module, the importing module binds the re-export directly; the author does not write a chain of qualifiers.

### 2.3 Attributes

```
attribute_list ::= { '@' IDENTIFIER }
```

An attribute list is a sequence of `@name` prefixes. The only recognized attribute is `@export`. An attribute list may precede any of `import`, `enum`, `resource`, or `node`.

The parser accepts any identifier after `@` and any number of attributes on any declaration. Sema enforces both the set of recognized attributes and the declarations they may appear on:

- `@export` is meaningful on `resource` only. On `import`, `enum`, or `node` it is an error (`Attr_ExportOnImport`, `Attr_ExportOnEnum`, `Attr_ExportOnNode`).
- An unrecognized attribute is an error (`Attr_Unknown`).
- The same attribute twice on one declaration is an error (`Attr_Duplicate`).

`@deprecate` is **not** part of the format. A future need for deprecation is handled by a special comment convention (not defined here) or by a future attribute. The grammar reserves the syntactic space; no attribute beyond `@export` is recognized today.

### 2.4 Enums

```
enum_decl ::= attribute_list 'enum' IDENTIFIER '{' enum_member_list '}'
enum_member_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]
```

Trailing comma allowed. Duplicate members within one enum are a semantic error.

`enum` declarations are typically host-provided. The parser accepts them; the host registry treats them as authoritative.

**Enum members are resolved to host-provided integer values at compile time.** The script author writes `Key.W`; Sema resolves it to the integer the host registered for that member. The graph stores the integer, not the name. At runtime, host node implementations receive the integer directly. The script never sees the integer.

Enum values are compared by integer value. Two members of the same enum that the host assigns the same integer are a host registration error, not a format error.

### 2.5 Resources

```
resource_decl  ::= attribute_list 'resource' IDENTIFIER '{' { resource_field } '}'
resource_field ::= IDENTIFIER ':' type_id [ '=' value ]

type_id        ::= [ IDENTIFIER '::' ] IDENTIFIER
```

A resource field has a name, a type, and an optional default. A field with no default is zero-initialized.

The default is a `value` (§2.7), not only a literal, so enum member access such as `Key.A` or `Direction.North` is accepted. The grammar allows any value; Sema enforces that the default is meaningful for the field's type.

`type_id` allows one level of module qualification, using `::` as the separator: `Key` from the local scope, `core::Key` from an import bound as `core`.

```
@export
resource PlayerConfig {
    speed:      float = 200.0
    jump_force: float = -400.0
    max_hp:     int   = 100
    key_left:   Key   = Key.A
}

resource PlayerState {
    hp:   int  = 100
    dead: bool = false
}
```

### 2.6 Nodes

```
node_decl    ::= attribute_list 'node' IDENTIFIER '=' node_expr [ 'on' trigger_list ]
node_expr    ::= NodeType '(' [ arg_list ] ')'
NodeType     ::= [ IDENTIFIER '::' ] IDENTIFIER
arg_list     ::= arg { ',' arg } [ ',' ]
arg          ::= value
trigger_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]
```

A node is `node <name> = <NodeType>(<args>)`, optionally followed by `on <trigger>, <trigger>, ...`.

```
node speed       = Float32Node(200.0)
node input_x     = InputAxis1D(Key.A, Key.D)
node body        = BodyNode(player)
node on_hit      = OnCollision(body, "hazard")
node play_sfx    = PlaySound("hit.wav") on on_hit
node damage      = Damage(body, 10) on on_hit, on_other
```

`NodeType` may be qualified to reach a node type from an imported module, using `::`:

```
import health
node h = health::Health(100)
```

### 2.7 Values

```
value ::= literal
        | IDENTIFIER
        | IDENTIFIER '.' IDENTIFIER
        | node_expr

literal ::= INT_LIT
          | FLOAT_LIT
          | STRING_LIT
          | CHAR_LIT
          | BOOL_LIT
          | NIL_LIT
```

A `value` is what may appear as an argument to a node or as the default of a resource field.

A bare literal is equivalent to a primitive node whose argument is that literal: `200.0` ≡ `Float32Node(200.0)`, `10` ≡ `Int32Node(10)`, `"hit.wav"` ≡ `StringNode("hit.wav")`.

An `IDENTIFIER` alone refers to a node or a resource by name.

An `IDENTIFIER '.' IDENTIFIER` is a field access. It covers:

- A resource field: `Config.speed`.
- An enum member: `Key.W`, `Direction.North`.
- A node output: `player_health.current`.

The parser produces a `FieldAccessValueAST`; Sema resolves the object's kind and reads the appropriate field, member, or output.

A `node_expr` at value position creates an inline node. It is valid but discouraged; a named node is more readable and reusable.

**Dot vs. double-colon.** A dot (`.`) is field access. A double-colon (`::`) is module qualification. They never mix in one name: `Key.W` is an enum member; `core::Key` is a qualified type. `core::Key.W` is not expressible, because an imported declaration is reached through the alias only at the type or node-type position, not at the value position. A value reaches a declaration by its bare imported name: `Key.W`.

### 2.8 The type system

The format recognizes three kinds of type. Every type used anywhere in a Lucid file falls into one of them.

**Primitive types.** `bool`, `char`, `string`, and the sized integer and float types (`int8` through `int64`, `uint8` through `uint64`, `float32`, `float64`). A primitive is a value: it can be read, assigned, passed as an argument, and stored in a resource.

**Enum types.** A named set of members declared by the host (`Key`, `Direction`). An enum member is a value, in the same sense as a primitive: it can be read, assigned, passed, and stored.

**Handle types.** Named, opaque references declared by the host (`BodyRef`, `TextureRef`, `SoundRef`, `SpriteRef`, `Array`, and any host-specific handles). A handle is a value: it can be read, assigned, passed, and stored. Handles are references — passing a handle shares the underlying resource rather than copying it. Every handle type has `nil` as a valid value, meaning "no resource."

**Trigger sources.** A trigger source is anything an `on` clause can target. One thing is a trigger source:

- A trigger node: a `node_decl` whose node type has kind `Trigger`.

**Handle creation, lifetime, and identity.** Handles are created by host value nodes (`BodyNode`, `LoadTexture`, `IntArrayNewNode`). They are passed as node arguments, stored in resources, and compared for equality. Two handles to the same resource compare equal; two handles to different resources compare unequal. Handles are valid until the underlying resource is freed. The script cannot free a handle directly; the host frees resources through action nodes, and the host defines what happens to a stale handle.

**Handle types are distinct.** `BodyRef` is not `TextureRef`. Passing one where the other is expected is a type error at Sema time.

**The rule in one line.** Every type denotes a value. Trigger sources are trigger nodes; they are not values and are legal only as the target of an `on` clause.

---

## 3. Notes on the grammar

### 3.1 No semicolons

A declaration ends at the first token that cannot continue it. Newlines are whitespace. Two declarations on one line require no separator, because a `node` declaration ends when the next token is not `on`, and a `resource` declaration ends at its closing brace.

### 3.2 No operators

Values are literals, identifiers, field accesses, and inline nodes. There is no `a + b`. If the graph needs addition, it uses `AddNode(a, b)`.

### 3.3 Order-independent name resolution

Declarations may reference each other in any order. Sema performs two passes: a collect pass that builds the symbol table, and a resolve pass that checks every reference. Order matters only for execution.

### 3.4 LL(1) parseability

Every production is uniquely determined by its first token. The parser needs one token of lookahead to decide:

- `top_decl`: the first token identifies the declaration kind. If it is `@`, the parser reads the attribute list and then the declaration keyword. If it is `import`, `enum`, `resource`, or `node`, the parser reads the declaration directly.
- `value`: the first token identifies the value kind (literal, identifier, or node expression start). An `IDENTIFIER` may be followed by `.` (field access) or `(` (node expression); one token of lookahead after the identifier decides.
- `node_decl` followed by `on` vs. the next declaration: after the argument list, the parser checks whether the next token is `on`. If it is, it parses a trigger list; if not, the node declaration ends.
- `resource_field`: after the field name and `:`, the parser reads a `type_id`. A `type_id` is `IDENTIFIER` or `IDENTIFIER '::' IDENTIFIER`. Because `::` is a distinct token from `:`, the field separator and the qualifier never collide. The parser reads `IDENTIFIER`, then either `:` (field separator; the type follows) or `::` (qualifier; the type name follows). One token of lookahead suffices.

No backtracking is required.

### 3.5 No expression parser

There is no expression grammar. A `value` is one of four forms, each beginning with a distinct token class. No precedence, no associativity, no parentheses for grouping.

### 3.6 No statement grammar

There are no statements. The top level contains only declarations. There is no block structure beyond the braces that delimit enum and resource bodies.

---

## 4. Open questions and gaps

The following are known gaps in this grammar and the surrounding design. They are documented here rather than left implicit.

### 4.1 `type_id` depth

`type_id ::= [ IDENTIFIER '::' ] IDENTIFIER` allows one level of module qualification. It does not allow deeper paths. If a module re-exports a type from a third module, this grammar cannot express it directly.

**Open question:** Is one level sufficient? The import model (§2.2) says an imported declaration is reached through a single alias. If a re-export is needed, the re-exporting module binds the name, and the importing module binds that binding under a new alias. Under that model, one level is sufficient. If re-export chains are needed, `type_id` grows to `IDENTIFIER { '::' IDENTIFIER }`, and `TypeIdAST`'s `qualifier` field becomes a span.

### 4.2 Qualified value access

A value reaches a declaration by its bare imported name: `Key.W`. It cannot reach a declaration through a module alias: `core::Key.W` is not expressible. This is deliberate. The dot is field access; the double-colon is module qualification; mixing them in one name would require the parser to decide which `::` and which `.` belong to which name.

**Open question:** Is bare-name access sufficient for values? The import model says yes, because the importing module binds the exported name directly. If a value must reach a shadowed name, the author uses `as` on the import to bind a distinct alias, and then reaches the alias-qualified type at the type position (`k::Key`) and the bare member at the value position (`Key.W`).

### 4.3 Attribute arguments

The grammar allows `@IDENTIFIER` only — no `@IDENTIFIER(args)`. This is deliberate: only `@export` exists, and it takes no arguments. `@deprecate` is not part of the format; a future deprecation mechanism is handled by a special comment convention or a future attribute, deferred until needed.

**Open question:** Is this acceptable, or should attribute arguments be reserved syntactically now to avoid a breaking change later? Reserving `@IDENTIFIER '(' ... ')'` in the grammar would cost one production and one AST field; not reserving it means a future attribute with arguments is a breaking grammar change. The current grammar does not reserve it.

### 4.4 Semicolons

There is no semicolon in the grammar. This is deliberate, but it means two declarations on one line require the parser to end the first at the correct token. The LL(1) property makes this unambiguous, but it also means a malformed line can cascade errors.

**Open question:** Is the optional-semicolon convention (allow `;` as a no-op) worth adding for error recovery? The current grammar does not include it.

### 4.5 `node_expr` inside `value`

The grammar allows `node_expr` inside `value`, which means an inline node can appear as an argument. This is valid but can produce confusing error messages when a nested inline node has a type mismatch.

**Open question:** Should inline node construction be restricted to a single level, or allowed at arbitrary depth? The current grammar allows arbitrary nesting.

### 4.6 Comments inside declarations

The grammar does not describe where comments may appear. In practice, comments are ignored everywhere between tokens. This is not explicitly stated.

**Open question:** Should the grammar specify that comments may appear between any two tokens, or is the current "lexer strips comments" approach sufficient?

### 4.7 Trailing commas

The grammar allows trailing commas in `enum_member_list`, `arg_list`, and `trigger_list`. Resource fields are not comma-separated, so the question does not arise there.

**Open question:** Is allowing trailing commas in all comma-separated lists the right convention? The current grammar allows it consistently.

### 4.8 Registry stability across builds

The registry assigns numeric IDs to node types and types. These IDs are used in `.lucgraph` files. If the registry changes between the build that serialized a graph and the build that loads it, the graph may not load.

**Open question:** Should `.lucgraph` files carry a registry fingerprint, so that mismatches are reported clearly rather than causing a silent misbinding? The current spec does not describe such a check.

### 4.9 Plugin-registered node types

The registry convention says registration happens once at startup, before any graph is loaded. If plugins may register node types, they must do so during startup, before the first graph load.

**Open question:** What is the interface for plugin registration? The current spec does not describe it.

---

## 5. The complete grammar, consolidated

For reference, here is the entire grammar without the annotations:

```
program     ::= { top_decl }

top_decl    ::= import_decl
              | enum_decl
              | resource_decl
              | node_decl

import_decl ::= attribute_list 'import' module_path [ 'as' IDENTIFIER ]
module_path ::= IDENTIFIER { '.' IDENTIFIER }

attribute_list ::= { '@' IDENTIFIER }

enum_decl ::= attribute_list 'enum' IDENTIFIER '{' enum_member_list '}'
enum_member_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]

resource_decl  ::= attribute_list 'resource' IDENTIFIER '{' { resource_field } '}'
resource_field ::= IDENTIFIER ':' type_id [ '=' value ]

node_decl    ::= attribute_list 'node' IDENTIFIER '=' node_expr [ 'on' trigger_list ]
node_expr    ::= NodeType '(' [ arg_list ] ')'
NodeType     ::= [ IDENTIFIER '::' ] IDENTIFIER
arg_list     ::= arg { ',' arg } [ ',' ]
arg          ::= value
trigger_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]

type_id ::= [ IDENTIFIER '::' ] IDENTIFIER

value ::= literal
        | IDENTIFIER
        | IDENTIFIER '.' IDENTIFIER
        | node_expr

literal ::= INT_LIT
          | FLOAT_LIT
          | STRING_LIT
          | CHAR_LIT
          | BOOL_LIT
          | NIL_LIT
```

---

## Summary of what changed in this version

**`::` replaces `:` for module qualification.** `core::Key`, `health::Health`. The single `:` remains only as the resource-field separator (`name: type`). The two are distinct tokens, so there is no ambiguity and no LL(2) anywhere.

**Parser, lexer, and Sema sections removed.** §5 (lexer sketch), §6 (parser output), and §7 (Sema's job) are gone. They were implementation notes, not grammar. What remains is the lexical grammar, the syntactic grammar, grammar notes, open questions, and the consolidated grammar.

**LL(1) is restored everywhere.** §3.4 no longer needs an LL(2) exception. The resource-field type position is unambiguous because `:` and `::` are different tokens.

**§4.1 (`type_id` depth) and §4.2 (qualified value access) are reframed.** They now ask whether one level of `::` qualification is sufficient, and whether bare-name value access is enough. The answers depend on the import model, which §2.2 states.

**§4.3 (attribute arguments) is updated.** It now explicitly says `@deprecate` is not part of the format and that the decision to add it is deferred.

**`[` `]` still reserved.** §1.6 keeps them in the punctuation set with a note. No production uses them.

---

## What is still stale in the other files

These are the remaining fixes, in actual-code form, when you want them:

1. **`DeclAST.hpp` — `ResourceFieldAST` constructor.** Takes `LiteralValueAST*`; should take `BaseAST*`, because the default is a `value` and `Key.A` is a field access.
2. **`DiagCode.hpp` — `Event_PortNotAllowed` (5303) and `Event_OutputNotTrigger` (5304).** Composite-era. Remove.
3. **`DiagCode.hpp` — attribute comment.** Now that `@export` is legal on all four declarations syntactically and rejected by Sema on three, the comment block should say so.
4. **`ValueAST.hpp` — `"-7"` example.** Not a valid lexeme. Change to `"7"` or `"42"`.
5. **`TypeAST.hpp` and `ValueAST.hpp` — `TypeIdAST` doc.** The separator is now `::`, not `.`. Update the examples to `core::Key`.
6. **`Tokens.hpp` — `COLON_COLON`.** Add a new token type for `::`, and a lexer rule that distinguishes it from `:`.
7. **`Diagnostic.cpp` — "Phase 1 stub" comment.** Stale. Remove.
8. **`AttributeAST.hpp` — doc.** Note that `@export` is syntactically allowed on all declarations and semantically resource-only.

Tell me which of these to write next, and I'll do it in actual-code form.