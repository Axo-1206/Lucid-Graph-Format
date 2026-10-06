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
node     composite
on       input    output
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
( ) { } [ ] , . : = @
```

`@` is the attribute sigil. It is always followed by an identifier and never appears alone. The only attribute the format recognizes is `@export`.

`@export` is lexed as two tokens: `@` and the identifier `export`. The parser combines them; Sema interprets `export` as the visibility attribute.

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
              | composite_decl
```

The top level contains only declarations. There are no top-level statements. Nothing runs at load time.

### 2.2 Imports

```
import_decl ::= 'import' module_path [ 'as' IDENTIFIER ]
module_path ::= IDENTIFIER { '.' IDENTIFIER }
```

`import core.keys` loads `core/keys.lucid` and binds it to the local name `keys`.
`import core.keys as k` binds it to `k`.

An `import_decl` may appear anywhere among the top-level declarations of a module.

### 2.3 Attributes

```
attribute_list ::= { '@' IDENTIFIER }
```

An attribute list is a sequence of `@name` prefixes. The only recognized attribute is `@export`. An attribute list may precede any of `enum`, `resource`, `node`, or `composite`. Attributes on any other declaration are a semantic error.

### 2.4 Enums

```
enum_decl ::= 'enum' IDENTIFIER '{' enum_member_list '}'
enum_member_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]
```

Trailing comma allowed. Duplicate members within one enum are a semantic error.

`enum` declarations are typically host-provided. The parser accepts them; the host registry treats them as authoritative.

**Enum members are resolved to host-provided integer values at compile time.** The script author writes `Key.W`; Sema resolves it to the integer the host registered for that member. The graph stores the integer, not the name. At runtime, host node implementations receive the integer directly. The script never sees the integer.

Enum values are compared by integer value. Two members of the same enum that the host assigns the same integer are a host registration error, not a format error.

### 2.5 Resources

```
resource_decl  ::= attribute_list 'resource' IDENTIFIER '{' { resource_field } '}'
resource_field ::= IDENTIFIER ':' type_id [ '=' literal ]

type_id        ::= IDENTIFIER [ '.' IDENTIFIER ]
```

A resource field has a name, a type, and an optional default. A field with no default is zero-initialized.

`type_id` allows dotted qualification for types from imported modules: `Key` from the local scope, `core.Key` from an import.

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
node_decl    ::= 'node' IDENTIFIER '=' node_expr [ 'on' trigger_list ]
node_expr    ::= NodeType '(' [ arg_list ] ')'
NodeType     ::= IDENTIFIER [ '.' IDENTIFIER ]
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

`NodeType` may be dotted to reach a node type from an imported module:

```
import health
node h = health.Health(100)
```

### 2.7 Composites

```
composite_decl ::= attribute_list 'composite' IDENTIFIER '{'
                     [ input_block ]
                     [ output_block ]
                     { composite_body_decl }
                   '}'

input_block  ::= 'input' '{' { composite_field } '}'
output_block ::= 'output' '{' { composite_output } '}'

composite_field  ::= IDENTIFIER ':' type_id
composite_output ::= IDENTIFIER ':' type_id '=' value

composite_body_decl ::= import_decl
                      | enum_decl
                      | resource_decl
                      | node_decl
```

An `input` block declares inputs. An `output` block declares outputs. Each output has a name, a type, and a right-hand side that is a `value` referring to something inside the composite body.

Outputs are of two kinds, distinguished by type. A data output has a primitive, enum, handle, or array type; its right-hand side is a resource field or a node output, and outside code reads it as a value. An event output has type `Event`; its right-hand side must be a trigger node inside the composite, and outside code subscribes to it with `on`.

`Event` is not a value type. See §2.9 for the full rules.

**Composites are declared at module level only.** A composite may not be declared inside another composite. A composite may use another composite — whether declared in the same module or imported — which is ordinary composition. A private helper composite is achieved by declaring it at module level without `@export`.

```
@export
composite Health {
    input  { max: int }

    output {
        current:  int   = State.current
        percent:  float = DivideNode(State.current, State.max)
        on_death: Event = dead
        on_hurt:  Event = hurt
    }

    resource State {
        current: int = 0
        max:     int = 0
    }

    node init_max = SetOnStart(State.max, max)
    node init_cur = SetOnStart(State.current, max)

    node on_hurt_event = OnEvent("hurt")
    node apply         = SubtractNode(State.current, on_hurt_event.amount)

    node check = LessNode(State.current, 1)
    node dead  = When(check)
    node hurt  = When(on_hurt_event)
}
```

Used from outside:

```
import health

node player_health = health.Health(100)

-- Data outputs: read as values
node hp_text = DrawText(10, 10, player_health.current)
node hp_bar  = DrawRect(10, 30, player_health.percent, 8) on EveryFrame()

-- Event outputs: subscribed to with `on`
node respawn   = RespawnPlayer() on player_health.on_death
node death_sfx = PlaySound("death.wav") on player_health.on_death
node hurt_sfx  = PlaySound("hurt.wav") on player_health.on_hurt
```

The type column distinguishes data outputs (`int`, `float`) from event outputs (`Event`). Both are declared in the same block, in the same syntax.

The `output` block references `State`, `dead`, and `hurt`, which are declared in the body below. Name resolution is order-independent (§4.3).

### 2.8 Values

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

A `value` is what may appear as an argument to a node, as the right-hand side of an output binding, or as the default of a resource field.

A bare literal is equivalent to a primitive node whose argument is that literal: `200.0` ≡ `Float32Node(200.0)`, `10` ≡ `Int32Node(10)`, `"hit.wav"` ≡ `StringNode("hit.wav")`.

An `IDENTIFIER` alone refers to a node, a resource, or a composite input by name. An `IDENTIFIER.IDENTIFIER` is field access: `Config.speed`, `input_jump.value`, `player_health.current`, `Key.W`.

A `node_expr` at value position creates an inline node. It is valid but discouraged; a named node is more readable and reusable.

---

### 2.9 The type system

The format recognizes four kinds of type. Every type used anywhere in a Lucid file falls into one of them.

**Primitive types.** `bool`, `char`, `string`, and the sized integer and float types (`int8` through `int64`, `uint8` through `uint64`, `float32`, `float64`). A primitive is a value: it can be read, assigned, passed as an argument, and stored in a resource.

**Enum types.** A named set of members declared by the host (`Key`, `Direction`). An enum member is a value, in the same sense as a primitive: it can be read, assigned, passed, and stored.

**Handle types.** Named, opaque references declared by the host (`BodyRef`, `TextureRef`, `SoundRef`, `SpriteRef`, `Array`, and any host-specific handles). A handle is a value: it can be read, assigned, passed, and stored. Handles are references — passing a handle shares the underlying resource rather than copying it. Every handle type has `nil` as a valid value, meaning "no resource."

**Event type.** `Event` is a single type shared by every trigger source in the format. It is not a value. A name whose type is `Event` refers to a trigger, not to a data value. Its only legal uses are:

- As the type of a composite output, exposing a trigger from inside the composite.
- As the target of an `on` clause, subscribing an action node to the trigger.

`Event` is not valid in any other position:

- Not a resource field type. Resources hold data; an event is not data.
- Not a composite input type. A composite cannot accept an event; it can only expose one.
- Not a node argument type. Node types do not take events as arguments. A trigger source is connected to an action through an `on` clause, not by being passed.

**Trigger sources.** Three things can be a trigger source: a trigger node, a composite's `Event`-typed output, and a host-provided `Event` value. All three may be the right-hand side of an `on` clause. Nothing else may.

**Handle creation, lifetime, and identity.** Handles are created by host value nodes (`BodyNode`, `LoadTexture`, `IntArrayNewNode`). They are passed as node arguments, stored in resources, and compared for equality. Two handles to the same resource compare equal; two handles to different resources compare unequal. Handles are valid until the underlying resource is freed. The script cannot free a handle directly; the host frees resources through action nodes, and the host defines what happens to a stale handle.

**Handle types are distinct.** `BodyRef` is not `TextureRef`. Passing one where the other is expected is a type error at Sema time.

**The rule in one line.** Every type except `Event` denotes a value. `Event` denotes a trigger source, and is legal only as a composite output type and as the target of an `on` clause.

---

## 3. The complete grammar in one block

```
-- ─── Program ────────────────────────────────────────────────────

program     ::= { top_decl }

top_decl    ::= import_decl
              | enum_decl
              | resource_decl
              | node_decl
              | composite_decl

-- ─── Imports ────────────────────────────────────────────────────

import_decl ::= 'import' module_path [ 'as' IDENTIFIER ]
module_path ::= IDENTIFIER { '.' IDENTIFIER }

-- ─── Attributes ─────────────────────────────────────────────────

attribute_list ::= { '@' IDENTIFIER }

-- ─── Enums ──────────────────────────────────────────────────────

enum_decl ::= 'enum' IDENTIFIER '{' enum_member_list '}'
enum_member_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]

-- ─── Resources ──────────────────────────────────────────────────

resource_decl  ::= attribute_list 'resource' IDENTIFIER '{' { resource_field } '}'
resource_field ::= IDENTIFIER ':' type_id [ '=' literal ]

-- ─── Nodes ──────────────────────────────────────────────────────

node_decl    ::= 'node' IDENTIFIER '=' node_expr [ 'on' trigger_list ]
node_expr    ::= NodeType '(' [ arg_list ] ')'
NodeType     ::= IDENTIFIER [ '.' IDENTIFIER ]
arg_list     ::= arg { ',' arg } [ ',' ]
arg          ::= value
trigger_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]

-- ─── Composites ─────────────────────────────────────────────────

composite_decl ::= attribute_list 'composite' IDENTIFIER '{'
                     [ input_block ]
                     [ output_block ]
                     { composite_body_decl }
                   '}'

input_block  ::= 'input' '{' { composite_field } '}'
output_block ::= 'output' '{' { composite_output } '}'

composite_field  ::= IDENTIFIER ':' type_id
composite_output ::= IDENTIFIER ':' type_id '=' value

composite_body_decl ::= import_decl
                      | enum_decl
                      | resource_decl
                      | node_decl

-- ─── Types ──────────────────────────────────────────────────────

type_id ::= IDENTIFIER [ '.' IDENTIFIER ]

-- ─── Values ─────────────────────────────────────────────────────

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

## 4. Notes on the grammar

### 4.1 No semicolons

A declaration ends at the first token that cannot continue it. Newlines are whitespace. Two declarations on one line require no separator, because a `node` declaration ends when the next token is not `on`, and a `resource` declaration ends at its closing brace.

### 4.2 No operators

Values are literals, identifiers, field accesses, and inline nodes. There is no `a + b`. If the graph needs addition, it uses `AddNode(a, b)`.

### 4.3 Order-independent name resolution

Declarations may reference each other in any order. Sema performs two passes: a collect pass that builds the symbol table, and a resolve pass that checks every reference. Order matters only for execution (§8 of the spec).

### 4.4 LL(1) parseability

Every production is uniquely determined by its first token. The parser needs one token of lookahead to decide:

- `top_decl`: the first token identifies the declaration kind (`import`, `enum`, `resource`, `node`, `composite`, `@`).
- `value`: the first token identifies the value kind (literal, identifier, or node expression start).
- `node_decl` followed by `on` vs. the next declaration: after the argument list, the parser checks whether the next token is `on`. If it is, it parses a trigger list; if not, the node declaration ends.

No backtracking is required.

### 4.5 No expression parser

There is no expression grammar. A `value` is one of four forms, each beginning with a distinct token class. No precedence, no associativity, no parentheses for grouping.

### 4.6 No statement grammar

There are no statements. The top level contains only declarations. Composites contain only declarations. There is no block structure beyond the braces that delimit declarations.

---

## 5. Lexer sketch

The lexer recognizes:

- Keywords: `import`, `from`, `enum`, `resource`, `node`, `composite`, `on`, `input`, `output`.
- Identifiers.
- Literals: integers (decimal, hex, binary, octal), floats, strings, chars, `true`, `false`, `nil`.
- Punctuation: `( ) { } [ ] , . : = @`.
- Comments: line (`--`) and block (`/- -/`).

The only disambiguation is `--` versus a hypothetical `-`, but `-` is not a token in this language (there are no operators), so `--` unambiguously begins a comment. `.` is used for field access and in float literals, disambiguated by whether a digit precedes and a digit follows.

```
Token Lexer::next() {
    skip_whitespace_and_comments();

    char c = peek();
    if (isalpha(c) || c == '_') return lex_identifier_or_keyword();
    if (isdigit(c))             return lex_number();
    if (c == '"')               return lex_string();
    if (c == '\'')              return lex_char();

    switch (c) {
        case '(': advance(); return Token::LParen;
        case ')': advance(); return Token::RParen;
        case '{': advance(); return Token::LBrace;
        case '}': advance(); return Token::RBrace;
        case '[': advance(); return Token::LBracket;
        case ']': advance(); return Token::RBracket;
        case ',': advance(); return Token::Comma;
        case '.': advance(); return Token::Dot;
        case ':': advance(); return Token::Colon;
        case '=': advance(); return Token::Equals;
        case '@': advance(); return Token::At;
        default:  error("unexpected character");
    }
}
```

---

## 6. Parser output

The parser produces a `Module` — a list of declarations. It does not resolve names, check types, or expand composites.

```cpp
struct Module {
    std::vector<ImportDecl>    imports;
    std::vector<EnumDecl>      enums;
    std::vector<ResourceDecl>  resources;
    std::vector<NodeDecl>      nodes;
    std::vector<CompositeDecl> composites;
};

struct NodeDecl {
    bool        is_export;
    std::string name;
    NodeExpr    expr;
    std::vector<std::string> triggers;   // from `on` clause
};

struct NodeExpr {
    std::string type_name;                // "MoveBody", "health.Health"
    std::vector<Value> args;
};

struct Value {
    enum class Kind { Literal, Identifier, FieldAccess, InlineNode };
    Kind kind;

    Literal     literal;                   // if Kind == Literal
    std::string identifier;                // if Kind == Identifier
    std::string field_object, field_name;  // if Kind == FieldAccess
    std::unique_ptr<NodeExpr> inline_node; // if Kind == InlineNode
};
```

The parser is purely syntactic. It builds the tree. Sema does everything else.

---

## 7. Sema's job

Sema consumes the parser's `Module` and produces a `Graph`. It performs:

1. **Import resolution.** Load every imported module, recurse.
2. **Symbol collection.** Build a symbol table of all declarations in each module.
3. **Type checking.** Every argument matches its slot; every field access resolves; every `on` names a trigger source; no name of type `Event` appears in a value position, a resource field, a composite input, or a node argument.
4. **Composite expansion.** Inline each composite use; rename internal nodes and resources uniquely.
5. **Cycle detection.** Value nodes must be acyclic; composite references must be acyclic.
6. **Dead code detection.** Value nodes must be used; action nodes must have `on`.
7. **Execution order computation.** Precompute `phase_order` (for action nodes) and `value_order` (for value nodes).

The output is a `Graph`, which the engine walks.

---

## 8. Open questions and gaps

The following are known gaps in this grammar and the surrounding design. They are documented here rather than left implicit.

### 8.1 Attribute list placement

The grammar shows `attribute_list` before `resource` and `composite` declarations. It also applies to `enum` when enums are script-declared, and to `node` when a node is exported from a module. The grammar does not currently show `attribute_list` before `node` or `enum`.

**Open question:** Should `@export` be allowed on `node` and `enum` declarations? If a module exports a composite, do its internal nodes need to be exported individually, or does the composite surface suffice? The current design assumes the latter, but this is not stated in the grammar.

### 8.2 `type_id` depth

`type_id ::= IDENTIFIER [ '.' IDENTIFIER ]` allows one level of module qualification. It does not allow deeper paths. If a module has nested modules, or if a type is reached through more than one level of import, this grammar cannot express it.

**Open question:** Is one level sufficient? If not, `type_id` should become `IDENTIFIER { '.' IDENTIFIER }`, matching `module_path`.

### 8.3 (Resolved) `emits` removed

The `emits` keyword and its grammar production have been removed. A composite's event outputs are declared in the `output` block with type `Event`. See §2.7 and §2.9.

### 8.4 Resource field access on imported resources

An imported resource's fields are accessed as `alias.Resource.field`. The grammar's `value` production allows only `IDENTIFIER '.' IDENTIFIER`, which is two components, not three.

**Open question:** Should `value` allow `IDENTIFIER '.' IDENTIFIER '.' IDENTIFIER` for imported resource field access? The current grammar does not.

### 8.4a Field access to an `Event`-typed name

An `Event`-typed composite output (`player_health.on_death`) is syntactically a field access, which is a `value` production. §2.9 states that `Event` is not a value, but the grammar does not currently forbid it in `value` position.

**Open question:** Should `value` be restricted so that a field access whose type is `Event` is a semantic error in value position, and legal only as the target of `on`? The grammar currently accepts it in both positions; Sema must reject the invalid one.

### 8.5 Composite input/output type qualification

`composite_field ::= IDENTIFIER ':' type_id` uses `type_id`, which allows one level of module qualification. If a composite's input type comes from a module that is imported under an alias, this works. If deeper qualification is needed, §8.2 applies.

### 8.6 Default values on composite inputs

The grammar does not allow defaults on composite inputs. This is a deliberate choice (documented in the spec), but it means every composite use must supply every input. If a common default is needed, it must be encoded as a module-level resource or a wrapper composite.

**Open question:** Is this acceptable at scale? Large composites with many inputs will be tedious to instantiate.

### 8.7 Enum declarations in composites

The grammar allows `enum_decl` inside a `composite_body_decl`. It is unclear whether an enum declared inside a composite has module scope or composite scope, and how it interacts with `@export`.

**Open question:** Are composite-internal enums legal, and if so, what is their visibility?

### 8.8 Attribute arguments

The grammar allows `@IDENTIFIER` only — no `@IDENTIFIER(args)`. This is deliberate (only `@export` exists), but if future attributes need arguments, the grammar will need to be extended.

**Open question:** Is this acceptable, or should attribute arguments be reserved syntactically now to avoid a breaking change later?

### 8.9 Semicolons

There is no semicolon in the grammar. This is deliberate, but it means two declarations on one line require the parser to end the first at the correct token. The LL(1) property makes this unambiguous, but it also means a malformed line can cascade errors.

**Open question:** Is the optional-semicolon convention (allow `;` as a no-op) worth adding for error recovery? The current grammar does not include it.

### 8.10 `node_expr` inside `value`

The grammar allows `node_expr` inside `value`, which means an inline node can appear as an argument. This is valid but can produce confusing error messages when a nested inline node has a type mismatch.

**Open question:** Should inline node construction be restricted to a single level, or allowed at arbitrary depth? The current grammar allows arbitrary nesting.

### 8.11 Comments inside declarations

The grammar does not describe where comments may appear. In practice, comments are ignored everywhere between tokens. This is not explicitly stated.

**Open question:** Should the grammar specify that comments may appear between any two tokens, or is the current "lexer strips comments" approach sufficient?

### 8.12 Empty input/output blocks

The grammar allows `input { }` and `output { }` — empty blocks. This is legal but meaningless.

**Open question:** Should empty blocks be a semantic error, or silently accepted? The current grammar allows them.

### 8.13 Trailing commas

The grammar allows trailing commas in `enum_member_list`, `arg_list`, and `trigger_list`, but not in `composite_field` (inside `input`/`output` blocks) or `resource_field` sequences. This is inconsistent.

**Open question:** Should trailing commas be allowed consistently? The current grammar is inconsistent.

### 8.14 Registry stability across builds

The registry assigns numeric IDs to node types and types. These IDs are used in `.lucgraph` files. If the registry changes between the build that serialized a graph and the build that loads it, the graph may not load.

**Open question:** Should `.lucgraph` files carry a registry fingerprint, so that mismatches are reported clearly rather than causing a silent misbinding? The current spec does not describe such a check.

### 8.15 Plugin-registered node types

The registry convention (§7 and the registry spec) says registration happens once at startup, before any graph is loaded. If plugins may register node types, they must do so during startup, before the first graph load.

**Open question:** What is the interface for plugin registration? The current spec does not describe it.

---

## 9. The complete grammar, consolidated

For reference, here is the entire grammar without the annotations:

```
program     ::= { top_decl }

top_decl    ::= import_decl
              | enum_decl
              | resource_decl
              | node_decl
              | composite_decl

import_decl ::= 'import' module_path [ 'as' IDENTIFIER ]
module_path ::= IDENTIFIER { '.' IDENTIFIER }

attribute_list ::= { '@' IDENTIFIER }

enum_decl ::= 'enum' IDENTIFIER '{' enum_member_list '}'
enum_member_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]

resource_decl  ::= attribute_list 'resource' IDENTIFIER '{' { resource_field } '}'
resource_field ::= IDENTIFIER ':' type_id [ '=' literal ]

node_decl    ::= 'node' IDENTIFIER '=' node_expr [ 'on' trigger_list ]
node_expr    ::= NodeType '(' [ arg_list ] ')'
NodeType     ::= IDENTIFIER [ '.' IDENTIFIER ]
arg_list     ::= arg { ',' arg } [ ',' ]
arg          ::= value
trigger_list ::= IDENTIFIER { ',' IDENTIFIER } [ ',' ]

composite_decl ::= attribute_list 'composite' IDENTIFIER '{'
                     [ input_block ]
                     [ output_block ]
                     { composite_body_decl }
                   '}'

input_block  ::= 'input' '{' { composite_field } '}'
output_block ::= 'output' '{' { composite_output } '}'

composite_field  ::= IDENTIFIER ':' type_id
composite_output ::= IDENTIFIER ':' type_id '=' value

composite_body_decl ::= import_decl
                      | enum_decl
                      | resource_decl
                      | node_decl

type_id ::= IDENTIFIER [ '.' IDENTIFIER ]

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
