/**
 * @file DiagCode.hpp
 * @brief The diagnostic code space: severity, category, and codes.
 *
 * ─── Design: codes describe what, not when ────────────────────────────────
 * A code names a *concept*, not a pipeline stage. "Unterminated string" is
 * the same code whether the lexer catches it at compile time or an LSP
 * reports it on a buffer.
 *
 * ─── Design: the code space reflects the grammar ──────────────────────────
 * The grammar has four top-level declaration forms (import, enum, resource,
 * node), a single type reference (`type_id`), and four value forms (literal,
 * identifier, field access, inline node). There are no functions, no
 * statements, no operators, no tables, no sequences, and no user-defined
 * types beyond the four declarations. The code space below covers exactly
 * what the grammar can produce, plus the small set of names Sema will need.
 *
 * ─── Bands ────────────────────────────────────────────────────────────────
 *   1000-1099  Lexical
 *   2000-2199  Syntax (general + per-declaration)
 *   3000-3099  Name resolution
 *   4000-4099  Value and type
 *   5000-5099  Attributes
 *   5100-5199  Imports
 *   5300-5399  Trigger rules
 *   7000-7099  Internal / panic / assertion
 *   8000-8299  Warnings
 *
 * Severity is a pure function of the code's range: 8000+ is a warning;
 * everything else is an error. Code 0 is reserved for free-text notes
 * and hints.
 */

#pragma once

#include <cstdint>

namespace lucid::diag
{

    // ─────────────────────────────────────────────────────────────────────────────
    // Severity
    // ─────────────────────────────────────────────────────────────────────────────

    enum class Severity : uint8_t
    {
        Hint = 0,
        Note = 1,
        Warning = 2,
        Error = 3,
        Fatal = 4,
    };

    inline const char *severityName(Severity s) noexcept
    {
        switch (s)
        {
        case Severity::Hint:
            return "HINT";
        case Severity::Note:
            return "NOTE";
        case Severity::Warning:
            return "WARNING";
        case Severity::Error:
            return "ERROR";
        case Severity::Fatal:
            return "FATAL";
        }
        return "UNKNOWN";
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Category
    // ─────────────────────────────────────────────────────────────────────────────

    enum class DiagCategory : uint8_t
    {
        Lexical,
        Syntax,
        Name,
        Type,
        Value,
        Attribute,
        Import,
        Internal, // free-text notes and hints (code 0)
        Warning,
        Unknown,
    };

    inline const char *categoryName(DiagCategory c) noexcept
    {
        switch (c)
        {
        case DiagCategory::Lexical:
            return "Lexical";
        case DiagCategory::Syntax:
            return "Syntax";
        case DiagCategory::Name:
            return "Name";
        case DiagCategory::Type:
            return "Type";
        case DiagCategory::Value:
            return "Value";
        case DiagCategory::Attribute:
            return "Attribute";
        case DiagCategory::Import:
            return "Import";
        case DiagCategory::Internal:
            return "Internal";
        case DiagCategory::Warning:
            return "Warning";
        case DiagCategory::Unknown:
            return "Unknown";
        }
        return "Unknown";
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // DiagCode
    // ─────────────────────────────────────────────────────────────────────────────

    /// @brief Unique diagnostic codes.
    ///
    /// Code 0 is reserved for free-text notes and hints.
    enum class DiagCode : uint32_t
    {

        // ═════════════════════════════════════════════════════════════════════════
        // LEXICAL (1000-1099)
        // ═════════════════════════════════════════════════════════════════════════

        Lex_InvalidCharacter = 1001,
        Lex_UnknownCharacter = 1002,
        Lex_UnterminatedString = 1003,
        Lex_UnterminatedRawString = 1004,
        Lex_UnterminatedCharLiteral = 1005,
        Lex_UnterminatedBlockComment = 1006,
        Lex_InvalidEscapeSequence = 1007,
        Lex_InvalidNumberLiteral = 1008,
        Lex_InvalidRadixLiteral = 1009,
        Lex_NewlineInString = 1010,

        // ═════════════════════════════════════════════════════════════════════════
        // SYNTAX (2000-2199)
        // ═════════════════════════════════════════════════════════════════════════

        // ─── General (2000-2049) ───────────────────────────────────────────

        Syntax_ExpectedIdentifier = 2001,
        Syntax_ExpectedToken = 2002,
        Syntax_UnexpectedToken = 2003,
        Syntax_ExpectedLiteral = 2004,
        Syntax_ExpectedBlock = 2005,
        Syntax_ExpectedClosing = 2006, // missing '}', ')', ']'
        Syntax_IncompleteDeclaration = 2007,

        // ─── Import (2050-2069) ────────────────────────────────────────────
        //
        // The import production is `attribute_list 'import' module_path`.
        // There is no `as` clause and no alias, so the only import-specific
        // syntax error is a missing or malformed module path.

        Syntax_ExpectedModulePath = 2050, // 'import' with no path

        // ─── Enum (2070-2079) ──────────────────────────────────────────────

        Syntax_ExpectedEnumName = 2070,
        Syntax_ExpectedEnumBody = 2071,
        Syntax_ExpectedEnumMember = 2072,

        // ─── Resource (2080-2099) ──────────────────────────────────────────

        Syntax_ExpectedResourceName = 2080,
        Syntax_ExpectedResourceBody = 2081,
        Syntax_ExpectedFieldName = 2082,
        Syntax_ExpectedFieldType = 2083,    // ':' present, type missing
        Syntax_ExpectedFieldDefault = 2084, // '=' present, value missing

        // ─── Node (2100-2119) ──────────────────────────────────────────────

        Syntax_ExpectedNodeName = 2100,
        Syntax_ExpectedNodeExpr = 2101, // '=' present, expression missing
        Syntax_ExpectedNodeType = 2102,
        Syntax_ExpectedNodeArgList = 2103,
        Syntax_ExpectedTriggerList = 2104, // 'on' present, no trigger

        // ─── Value (2160-2179) ─────────────────────────────────────────────

        Syntax_ExpectedValue = 2160,       // argument position, nothing valid
        Syntax_ExpectedFieldAccess = 2161, // '.' present, field name missing

        // ─── Attribute (2180-2199) ─────────────────────────────────────────

        Syntax_ExpectedAttributeName = 2180, // '@' present, no identifier
        Syntax_ExpectedAttributeList = 2181, // attribute list in wrong place
        Syntax_ExpectedDeclaration = 2182,   // attribute followed by non-decl

        // ═════════════════════════════════════════════════════════════════════════
        // NAME RESOLUTION (3000-3099)
        // ═════════════════════════════════════════════════════════════════════════
        //
        // Sema's territory. The formatter does not resolve names, but the
        // codes exist so that Sema's error reporting is defined in one place.

        Name_UndefinedModule = 3001,     // import path resolves to nothing
        Name_UndefinedType = 3002,       // type_id resolves to nothing
        Name_UndefinedNode = 3003,       // node reference resolves to nothing
        Name_UndefinedResource = 3004,   // resource reference resolves to nothing
        Name_UndefinedField = 3005,      // field access resolves to nothing
        Name_UndefinedTrigger = 3006,    // trigger name resolves to nothing
        Name_UndefinedEnumMember = 3007, // enum member resolves to nothing
        Name_Redeclaration = 3008,       // two declarations with same name
        Name_DuplicateEnumMember = 3009, // two members with same name
        Name_DuplicateTrigger = 3010,    // two triggers on one node

        // ═════════════════════════════════════════════════════════════════════════
        // VALUE AND TYPE (4000-4099)
        // ═════════════════════════════════════════════════════════════════════════

        Type_Mismatch = 4001,           // arg type does not match slot
        Type_ArgCountMismatch = 4002,   // too many or too few args
        Type_UnknownNodeType = 4003,    // NodeType not in registry
        Type_UnknownType = 4004,        // type_id not in registry
        Type_InvalidDefault = 4005,     // resource default type mismatch
        Type_InvalidBinding = 4006,     // binding mismatch between value and target
        Type_InvalidNodeArg = 4007,     // argument not a valid value
        Type_InvalidFieldAccess = 4008, // base is not field-accessible
        Type_Cycle = 4009,              // A cycle exists among the value nodes.

        Value_DuplicateFieldDefault = 4101, // resource with two defaults

        // ═════════════════════════════════════════════════════════════════════════
        // ATTRIBUTES (5000-5099)
        // ═════════════════════════════════════════════════════════════════════════
        //
        // The new grammar has exactly one recognized attribute: @export.
        // @export is legal on enum, resource, and node declarations. It
        // is not legal on an import (an import binds names into the
        // current module; it has no declaration another module can see).
        // Any other @name is an unknown attribute.

        Attr_Unknown = 5001,          // @name not recognized
        Attr_ExportOnImport = 5002,   // @export on import_decl
        Attr_ExportOnField = 5003,    // @export inside a declaration body
        Attr_Duplicate = 5004,        // @export twice on one decl
        Attr_ArgCountMismatch = 5005, // attribute takes no args
        Attr_InvalidArgValue = 5006,  // (reserved for future attributes)

        // ═════════════════════════════════════════════════════════════════════════
        // IMPORTS (5100-5199)
        // ═════════════════════════════════════════════════════════════════════════
        //
        // The formatter does not resolve imports; the CLI does, and Sema
        // consumes the result. The import production has no alias, so
        // there is no alias-collision code. A collision between two
        // imports' exported bare names is Name_Redeclaration (3008), not
        // an import-band code.

        Import_ModuleNotFound = 5101, // file does not exist
        Import_Circular = 5102,       // A imports B imports A
        Import_NotAFile = 5103,       // path resolves to a directory

        // ═════════════════════════════════════════════════════════════════════════
        // TRIGGER RULES (5300-5399)
        // ═════════════════════════════════════════════════════════════════════════
        //
        // Sema's rules about node subscription. The format has no Event
        // type, no ports, and no composite nodes; a trigger source is only
        // a trigger node, and an action node must have at least one `on`
        // clause.
        //
        //   - An `on` clause's target must be a trigger node.
        //   - An action node must have at least one `on` clause.

        Trigger_OnTargetNotTrigger = 5301,
        Trigger_ActionWithoutOn = 5302,

        // ═════════════════════════════════════════════════════════════════════════
        // INTERNAL / PANIC (7000-7099)
        // ═════════════════════════════════════════════════════════════════════════
        //
        // Compiler-internal failures. These are for the AST_ASSERT_MSG macro
        // in BaseAST.hpp and any other invariant failure. They are not
        // user-facing; when one fires, the compiler has a bug.

        Internal_Assertion = 7001,
        Internal_Unreachable = 7002,
        Internal_NotImplemented = 7003,

        // ═════════════════════════════════════════════════════════════════════════
        // WARNINGS (8000-8299)
        // ═════════════════════════════════════════════════════════════════════════

        Warn_UnusedImport = 8001,
        Warn_UnusedResource = 8002,
        Warn_UnusedNode = 8003, // value node never referenced
        Warn_UnusedEnum = 8004,
        Warn_DeadNode = 8005, // value node not referenced by any other node
        Warn_EmptyResource = 8006,
        Warn_TrailingComma = 8007, // stylistic; formatter normalizes

        Warn_Deprecated = 8100, // reserved for a future @deprecated
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // Derived properties
    // ─────────────────────────────────────────────────────────────────────────────

    inline constexpr uint32_t raw(DiagCode c) noexcept
    {
        return static_cast<uint32_t>(c);
    }

    inline constexpr DiagCategory categoryFromCode(DiagCode c) noexcept
    {
        const uint32_t v = raw(c);
        if (v == 0)
            return DiagCategory::Internal;
        if (v < 2000)
            return DiagCategory::Lexical;
        if (v < 3000)
            return DiagCategory::Syntax;
        if (v < 4000)
            return DiagCategory::Name;
        if (v < 5000)
            return DiagCategory::Type;
        if (v < 5100)
            return DiagCategory::Attribute;
        if (v < 5200)
            return DiagCategory::Import;
        if (v < 8000)
            return DiagCategory::Internal;
        if (v < 9000)
            return DiagCategory::Warning;
        return DiagCategory::Unknown;
    }

    inline constexpr Severity severityFromCode(DiagCode c) noexcept
    {
        return raw(c) >= 8000 ? Severity::Warning : Severity::Error;
    }

    inline constexpr bool isWarningCode(DiagCode c) noexcept
    {
        return raw(c) >= 8000 && raw(c) < 9000;
    }

    inline constexpr bool isErrorCode(DiagCode c) noexcept
    {
        return !isWarningCode(c) && raw(c) != 0;
    }

} // namespace lucid::diag