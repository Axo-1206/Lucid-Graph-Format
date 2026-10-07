/**
 * @file core/ast/BaseAST.hpp
 *
 * @responsibility The foundation of the AST. Defines BaseAST, the
 *                 visitor-free isa/as helpers, the ASTKind enum, the
 *                 AST_ASSERT_MSG macro, and the shared error-recovery
 *                 nodes.
 *
 * ─── Design: the AST is the compiler's only tree ──────────────────────────
 * Every stage of the frontend — parser, formatter, and any future Sema or
 * bytecode compiler — reads the same AST. There is no intermediate
 * representation between the AST and the formatter's output. The parser
 * produces the AST; every later stage walks it.
 *
 * ─── Design: the grammar has no expressions ───────────────────────────────
 * The Lucid Graph Format has no operator grammar. A value is one of four
 * forms (literal, identifier, field access, inline node), and a type is
 * one form (a possibly-dotted name). There is no ExprAST family base
 * because there is no expression hierarchy to base it on. Value nodes
 * derive directly from BaseAST.
 *
 * ─── Design: nilability, handles, and types live in the grammar's
 *            `type_id` and the host registry, not here ─────────────────────
 * The AST carries type references as names (TypeIdAST). It does not carry
 * resolved types. Sema, when it lands, will resolve a TypeIdAST to a
 * registry entry; that resolution is not on the AST.
 *
 * ─── Design: the AST holds no codegen facts ───────────────────────────────
 * No bytecode offsets, no resolved types, no binding IDs. Every fact that
 * a later pass computes lives on a later-pass object, not on the AST. The
 * reason: the AST outlives any single pass (the formatter and a future
 * compiler both read the same tree), so a fact that belongs to one pass
 * would be stale or wrong when another pass reads it.
 *
 * ─── Design: family forward declarations keep the graph acyclic ───────────
 * Every concrete AST node is forward-declared here. Family headers
 * (AttributeAST.hpp, TypeAST.hpp, ValueAST.hpp, DeclAST.hpp,
 * ModuleAST.hpp) include this file and define their concrete nodes. This
 * file never includes a family header, so the dependency graph is acyclic.
 */

#pragma once

#include "core/SourceLocation.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/ArenaSpan.hpp"
#include "core/memory/InternedString.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────────────
// Family forward declarations
// ─────────────────────────────────────────────────────────────────────────────

// AttributeAST.hpp
struct AttributeAST;

// TypeAST.hpp
struct TypeIdAST;

// ValueAST.hpp
struct NodeExprAST;
struct LiteralValueAST;
struct IdentifierValueAST;
struct FieldAccessValueAST;
struct InlineNodeValueAST;

// DeclAST.hpp
struct DeclAST;
struct ImportDeclAST;
struct EnumDeclAST;
struct EnumMemberAST;
struct ResourceDeclAST;
struct ResourceFieldAST;
struct NodeDeclAST;
struct CompositeDeclAST;
struct CompositeInputAST;
struct CompositeOutputAST;

// ModuleAST.hpp
struct ModuleAST;

// Error-recovery node
struct UnknownAST;

// ─────────────────────────────────────────────────────────────────────────────
// ASTKind
// ─────────────────────────────────────────────────────────────────────────────
//
// Compile-time tag on every node. Replaces RTTI with one integer
// comparison. Every concrete node declares `static constexpr ASTKind
// staticKind` and passes it to its constructor.
//
// The enum is ordered so nodes of the same family are contiguous. The
// ordering is an implementation detail; the isa/as helpers and the family
// base types are the contract.

enum class ASTKind : uint16_t
{

    // ─── Attribute ──────────────────────────────────────────────────────
    Attribute,

    // ─── Type ───────────────────────────────────────────────────────────
    TypeId,

    // ─── Values ─────────────────────────────────────────────────────────
    //
    // There is no ValueAST family base; the four value nodes are used
    // interchangeably through BaseAST*. The kinds are grouped for
    // readability, not for a family query.

    NodeExpr,
    LiteralValue,
    IdentifierValue,
    FieldAccessValue,
    InlineNodeValue,

    // ─── Declarations ───────────────────────────────────────────────────
    //
    // Decl is the family base; the eight concrete kinds follow it.
    // DeclAST's family query covers [Decl, CompositeOutput].

    Decl, // family base
    ImportDecl,
    EnumDecl,
    EnumMember,
    ResourceDecl,
    ResourceField,
    NodeDecl,
    CompositeDecl,
    CompositeInput,
    CompositeOutput,

    // ─── Root ───────────────────────────────────────────────────────────
    Module,

    // ─── Error recovery ─────────────────────────────────────────────────
    Unknown,
};

// ─────────────────────────────────────────────────────────────────────────────
// ASTKindOf — the exact kind for a concrete node
// ─────────────────────────────────────────────────────────────────────────────

/// The exact ASTKind a concrete node reports. Used when a caller needs
/// the precise kind (a switch, a serialization table, a test that checks
/// identity).
///
/// The primary template reads `T::staticKind`. It is a template rather
/// than a direct member access because `T` may be a forward-declared type
/// at the point where `isa<T>()` is instantiated; a template delays the
/// member lookup until instantiation, when `T` is complete.
template <typename T>
struct ASTKindOf
{
    static constexpr ASTKind value = T::staticKind;
};

// Explicit specializations for the family bases and any type that does
// not follow the `staticKind` convention. UnknownAST, BaseAST itself, and
// the family bases are all valid targets for isa<>; the specializations
// give them a value.

#define AST_KIND_OF(Type, Kind)                         \
    template <>                                         \
    struct ASTKindOf<Type>                              \
    {                                                   \
        static constexpr ASTKind value = ASTKind::Kind; \
    }

AST_KIND_OF(AttributeAST, Attribute);
AST_KIND_OF(TypeIdAST, TypeId);
AST_KIND_OF(NodeExprAST, NodeExpr);
AST_KIND_OF(LiteralValueAST, LiteralValue);
AST_KIND_OF(IdentifierValueAST, IdentifierValue);
AST_KIND_OF(FieldAccessValueAST, FieldAccessValue);
AST_KIND_OF(InlineNodeValueAST, InlineNodeValue);
AST_KIND_OF(DeclAST, Decl);
AST_KIND_OF(ImportDeclAST, ImportDecl);
AST_KIND_OF(EnumDeclAST, EnumDecl);
AST_KIND_OF(EnumMemberAST, EnumMember);
AST_KIND_OF(ResourceDeclAST, ResourceDecl);
AST_KIND_OF(ResourceFieldAST, ResourceField);
AST_KIND_OF(NodeDeclAST, NodeDecl);
AST_KIND_OF(CompositeDeclAST, CompositeDecl);
AST_KIND_OF(CompositeInputAST, CompositeInput);
AST_KIND_OF(CompositeOutputAST, CompositeOutput);
AST_KIND_OF(ModuleAST, Module);
AST_KIND_OF(UnknownAST, Unknown);

#undef AST_KIND_OF

// ─────────────────────────────────────────────────────────────────────────────
// ASTKindMatches — the family-aware match used by isa<T>() and as<T>()
// ─────────────────────────────────────────────────────────────────────────────
//
// `isa<T>()` answers "can this node be treated as a T?". For a concrete
// node, that is an exact-kind test. For a family base, it is a range
// test over the family's contiguous kinds.
//
// The primary template uses exact kind equality. Family bases are
// specialized.

template <typename T>
struct ASTKindMatches
{
    static bool check(ASTKind k) noexcept
    {
        return k == ASTKindOf<T>::value;
    }
};

/// DeclAST's family covers [Decl, CompositeOutput].
template <>
struct ASTKindMatches<DeclAST>
{
    static bool check(ASTKind k) noexcept
    {
        return k >= ASTKind::Decl && k <= ASTKind::CompositeOutput;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AST_ASSERT_MSG
// ─────────────────────────────────────────────────────────────────────────────
//
// Invariant check for compiler bugs. Unlike `assert`, this:
//   - always fires (not disabled by NDEBUG),
//   - prints file, line, function, and a caller-supplied message,
//   - aborts.
//
// Use ONLY for conditions that indicate a bug in the compiler itself.
// User-facing diagnostics go through DiagnosticEngine.
//
// The message is a string literal — no allocation, no formatting.

#define AST_ASSERT_MSG(cond, msg)                                     \
    do                                                                \
    {                                                                 \
        if (!(cond))                                                  \
        {                                                             \
            std::fprintf(stderr,                                      \
                         "\nAssertion failed: %s\n"                   \
                         "  File:     %s\n"                           \
                         "  Line:     %d\n"                           \
                         "  Function: %s\n"                           \
                         "  Message:  %s\n",                          \
                         #cond, __FILE__, __LINE__, __func__, (msg)); \
            std::abort();                                             \
        }                                                             \
    } while (0)

// ─────────────────────────────────────────────────────────────────────────────
// BaseAST
// ─────────────────────────────────────────────────────────────────────────────

/// @brief The root of the AST hierarchy.
///
/// Every node carries its ASTKind tag, its source location, and a
/// `hasSyntaxError` flag. Concrete nodes add their own fields.
struct BaseAST
{
    /// The compile-time tag. Set once by the concrete node's constructor
    /// and never changed.
    ASTKind kind;

    /// The node's source location. The convention is: a node's location
    /// is the location of the *first* token of the construct it
    /// represents. A compound node's location is the location of its
    /// leftmost or leading token.
    SourceLocation loc;

    /// True if the node was produced by a parser error-recovery path. A
    /// node with this flag set is structurally valid but represents
    /// source that had a syntax error; later passes skip it or treat its
    /// missing fields as unknowns. The flag propagates upward: a node
    /// whose subtree contains a marked node is itself marked.
    bool hasSyntaxError = false;

    explicit BaseAST(ASTKind k) : kind(k) {}
    virtual ~BaseAST() = default;

    template <typename T>
    bool isa() const { return ASTKindMatches<T>::check(kind); }

    template <typename T>
    T *as()
    {
        AST_ASSERT_MSG(ASTKindMatches<T>::check(kind),
                       "ASTKind mismatch in as<T>() — caller assumed the "
                       "wrong node type");
        return static_cast<T *>(this);
    }

    template <typename T>
    const T *as() const
    {
        AST_ASSERT_MSG(ASTKindMatches<T>::check(kind),
                       "ASTKind mismatch in as<T>() — caller assumed the "
                       "wrong node type");
        return static_cast<const T *>(this);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// UnknownAST — error-recovery placeholder
// ─────────────────────────────────────────────────────────────────────────────
//
// A parser error-recovery path that cannot produce a real node produces
// an UnknownAST with `hasSyntaxError = true`. Later passes recognize it
// and skip the construct it appears in rather than reporting a follow-on
// error for every field access on the bad node.

/// @brief A generic unknown node.
struct UnknownAST : BaseAST
{
    static constexpr ASTKind staticKind = ASTKind::Unknown;

    UnknownAST() : BaseAST(ASTKind::Unknown)
    {
        hasSyntaxError = true;
    }
};

/// @brief True if a node is null or one of the error-recovery placeholders.
///
/// A null pointer is treated as unknown so callers can write
/// `if (isUnknown(node))` without a separate null check.
inline bool isUnknown(const BaseAST *node)
{
    if (!node)
        return true;
    return node->kind == ASTKind::Unknown;
}