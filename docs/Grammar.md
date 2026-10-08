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
INT_LIT    ::= [ '+' | '-' ] ( DIGIT+ | '0x' HEX+ | '0b' BIN+ | '0o' OCT+ )
FLOAT_LIT  ::= [ '+' | '-' ] DIGIT+ '.' DIGIT+ [ ('e'|'E') ['+'|'-'] DIGIT+ ]
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

`INT_LIT` and `FLOAT_LIT` carry an optional leading sign. `-7` is a single token, not a `-` followed by `7`. The sign is part of the literal's text, and `LiteralValueAST::text` stores it as written (`"-7"`, `"+3.14"`, `"-0xFF"`).

The sign is optional and lexical, not syntactic. There is no unary-minus production and no operator grammar. A `-` that is not followed by a digit, an `x`, a `b`, or an `o` (for a radix prefix) is a lexical error (`Lex_InvalidCharacter`), because `-` has no other meaning in the language.

### 1.5 Comments

```
line_comment  ::= '--' { ANY_CHAR_EXCEPT_NEWLINE } NEWLINE
block_comment ::= '/-' { ANY_CHAR | block_comment } '-/'    -- nestable
```

Comments are ignored. There are no doc comments.

The lexer recognizes `--` before it tries to read a signed number, so `--7` is a line comment whose text is `7`, not a signed literal.

### 1.6 Punctuation and attributes

```
( ) { } [ ] , . : :: = @
```

`@` is the attribute sigil. It is always followed by an identifier and never appears alone. The only attribute the format recognizes is `@export`.

`@export` is lexed as two tokens: `@` and the identifier `export`. The parser combines them; Sema interprets `export` as the visibility attribute.

`[` and `]` are **reserved**. No production in the current grammar uses them; they are lexed so that a future array or index syntax does not require a lexer change. A `[` or `]` in source is a syntax error at the parser, not the lexer.

`:` is the resource-field separator (`name: type`). `::` is the module qualifier (`module::Name`). The two are distinct tokens; there is no ambiguity between them.

`+` and `-` are **not** punctuation. They appear only as the leading sign of a numeric literal. They are not tokens on their own; the lexer consumes them as part of the literal it is reading.

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
import_decl ::= attribute_list 'import' module_path
module_path ::= IDENTIFIER { '.' IDENTIFIER }
```

`import core.keys` loads `core/keys.lucid`.

An `import_decl` may appear anywhere among the top-level declarations of a module.

**The import model.** An import binds two things:

1. The module's **exported declarations**, into the importing module's scope, by their bare names.
2. A **module name**, which can be used as a `::` qualifier in type positions.

The module name is always the final segment of the module path. `import core.keys` binds the module name `keys`; `import engine.physics` binds `physics`. There is no alias and no `as` clause.

**Bare access.** An exported `enum`, `resource`, or `node` is reachable by its bare name after import. If `core.keys` exports an enum `Key`, then after `import core.keys`, the author writes `Key` for the type and `Key.W` for a member. The dot is field access; the bare name is what the import injects.

**Qualified access.** The module name is used as a `::` qualifier, in **type positions only**:

- A resource field's type: `keys::Key`.
- A node's type: `physics::Body`.

The qualifier is optional. It is useful when a bare name would collide with another declaration, or when the author wants to make the origin explicit.

**Values are always bare.** A value reaches a declaration by its bare name, never through a module qualifier. `Key.W` is a value; `keys::Key.W` is not expressible. This is deliberate: the dot is field access and the double-colon is module qualification, and a value cannot carry both. If a bare name would be ambiguous, the author resolves the ambiguity at the type position (using `keys::Key` for the type) and still writes the member bare (`Key.W`).

**Collisions.** Two imports that export the same bare name collide (`Name_Redeclaration`). The resolution is the module qualifier at the type position: `a::Key` and `b::Key` are distinct. A value-position reference to the colliding name is ambiguous; the current grammar does not provide a way to disambiguate it. See §4.2.

**Depth.** A qualified name has at most one qualifier. `keys::Key` is valid; `a::b::Key` is not. The import model guarantees that one level is enough: every module that imports a module binds its exported declarations bare, so an author never needs to write a chain of qualifiers to reach a re-export.

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

`type_id` allows one level of module qualification, using `::` as the separator: `Key` from the local scope, `keys::Key` from the module `core.keys`.

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
import engine.physics
node h = physics::Body(player)
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

A bare literal is equivalent to a primitive node whose argument is that literal: `200.0` ≡ `Float32Node(200.0)`, `10` ≡ `Int32Node(10)`, `"hit.wav"` ≡ `StringNode("hit.wav")`. A signed literal is a literal like any other: `-400.0` ≡ `Float32Node(-400.0)`.

An `IDENTIFIER` alone refers to a node or a resource by name.

An `IDENTIFIER '.' IDENTIFIER` is a field access. It covers:

- A resource field: `Config.speed`.
- An enum member: `Key.W`, `Direction.North`.
- A node output: `player_health.current`.

The parser produces a `FieldAccessValueAST`; Sema resolves the object's kind and reads the appropriate field, member, or output.

A `node_expr` at value position creates an inline node. It is valid but discouraged; a named node is more readable and reusable.

**Dot vs. double-colon.** A dot (`.`) is field access. A double-colon (`::`) is module qualification. They never mix in one name: `Key.W` is an enum member; `keys::Key` is a qualified type. `keys::Key.W` is not expressible, because an imported declaration is reached through the qualifier only at the type or node-type position, not at the value position. A value reaches a declaration by its bare imported name: `Key.W`.

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

Values are literals, identifiers, field accesses, and inline nodes. There is no `a + b`. If the graph needs addition, it uses `AddNode(a, b)`. A leading sign on a numeric literal is not an operator; it is part of the literal.

### 3.3 Order-independent name resolution

Declarations may reference each other in any order. Sema performs two passes: a collect pass that builds the symbol table, and a resolve pass that checks every reference. Order matters only for execution.

### 3.4 LL(1) parseability

Every production is uniquely determined by its first token. The parser needs one token of lookahead to decide:

- `top_decl`: the first token identifies the declaration kind. If it is `@`, the parser reads the attribute list and then the declaration keyword. If it is `import`, `enum`, `resource`, or `node`, the parser reads the declaration directly.
- `value`: the first token identifies the value kind (literal, identifier, or node expression start). An `IDENTIFIER` may be followed by `.` (field access) or `(` (node expression); one token of lookahead after the identifier decides. A signed literal is one token, so no special lookahead is needed.
- `node_decl` followed by `on` vs. the next declaration: after the argument list, the parser checks whether the next token is `on`. If it is, it parses a trigger list; if not, the node declaration ends.
- `resource_field`: after the field name and `:`, the parser reads a `type_id`. A `type_id` is `IDENTIFIER` or `IDENTIFIER '::' IDENTIFIER`. Because `::` is a distinct token from `:`, the field separator and the qualifier never collide. The parser reads `IDENTIFIER`, then either `:` (field separator; the type follows) or `::` (qualifier; the type name follows). One token of lookahead suffices.

No backtracking is required.

### 3.5 No expression parser

There is no expression grammar. A `value` is one of four forms, each beginning with a distinct token class. No precedence, no associativity, no parentheses for grouping.

### 3.6 No statement grammar

There are no statements. The top level contains only declarations. There is no block structure beyond the braces that delimit enum and resource bodies.

### 3.7 Signed literals

A numeric literal may carry a leading `+` or `-`. The sign is part of the literal token, not a separate operator. `-400.0` is one `FLOAT_LIT`, not a `-` followed by `400.0`. The lexer reads the sign when the next character is a digit (or a radix prefix), and stores the whole lexeme as the literal's text.

There is no unary-minus production and no operator grammar. `a - b` is not valid; a graph that needs subtraction uses `SubtractNode(a, b)`.

The `-` in `--` (a line comment) is not a literal sign. The lexer recognizes `--` before it tries to read a signed number, so `--7` is a line comment, not a signed literal.

---

## 4. Open questions and gaps

The following are known gaps in this grammar and the surrounding design. They are documented here rather than left implicit.

### 4.1 `type_id` depth

`type_id ::= [ IDENTIFIER '::' ] IDENTIFIER` allows one level of module qualification. It does not allow deeper paths. If a module re-exports a type from a third module, this grammar cannot express it directly.

**Open question:** Is one level sufficient? The import model (§2.2) says every module that imports a module binds its exported declarations bare, so an author never needs a chain of qualifiers. Under that model, one level is sufficient. If re-export chains must be expressible, `type_id` grows to `IDENTIFIER { '::' IDENTIFIER }`, and `TypeIdAST`'s `qualifier` field becomes a span.

### 4.2 Collisions at the value position

Two imports that export the same bare name collide. The resolution is the module qualifier at the type position: `a::Key` and `b::Key` are distinct. But a value reaches a declaration by its bare name, and there is no qualifier for a value. So a value-position reference to a colliding name is ambiguous, and the current grammar provides no way to disambiguate it.

**Open question:** Is this acceptable? Three responses are possible:

1. **Host guarantees unique exported names.** If the host registry refuses to register two node types, enums, or resources with the same exported name, collisions cannot occur, and the question is moot. This is the simplest resolution and probably the right one for a graph format where the host controls the vocabulary.
2. **Allow `::` in value position.** Widen `value` to permit `IDENTIFIER '::' IDENTIFIER '.' IDENTIFIER` (or a similar shape), so a value can be qualified. This reintroduces the mix of `::` and `.` that the current grammar avoids, and it forces `FieldAccessValueAST` to carry an optional qualifier.
3. **Add an import-rename mechanism.** A way to bind an imported declaration under a new bare name in the importing module. This is the `as` clause under a different shape, and it reintroduces the naming complexity that the current grammar removed.

The current grammar chooses none of these. It documents the gap and leaves it open.

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

### 4.10 Signed literals in radix forms

The grammar allows `-0xFF`, `-0b1010`, and `-0o17`. Is a sign meaningful for a radix literal? `0xFF` is unsigned in most languages; `-0xFF` is either `-(0xFF)` or a malformed token, depending on the language.

**Open question:** Should the sign be allowed on all radix forms, or only on decimal integers and floats? The current grammar allows it on all four.

### 4.11 Explicit `+` sign

The grammar allows an explicit `+` on a numeric literal (`+7`, `+3.14`). This is symmetric with `-` and costs one lexer branch. It is rarely useful; a reader who sees `+7` may wonder why the author wrote it.

**Open question:** Should `+` be allowed, or should the grammar accept only `-`? The current grammar allows both, for symmetry.

---

## 5. The complete grammar, consolidated

For reference, here is the entire grammar without the annotations:

```
program     ::= { top_decl }

top_decl    ::= import_decl
              | enum_decl
              | resource_decl
              | node_decl

import_decl ::= attribute_list 'import' module_path
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