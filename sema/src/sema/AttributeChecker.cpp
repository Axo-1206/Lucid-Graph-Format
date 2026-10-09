/// @file sema/src/sema/AttributeChecker.cpp
///
/// @brief Implementation of attribute validation.

#include "AttributeChecker.hpp"

#include "core/ast/AttributeAST.hpp"
#include "core/ast/DeclAST.hpp"
#include "core/diagnostics/DiagCode.hpp"

#include <vector>

using namespace lucid::diag;

namespace lucid::sema
{

    void checkAttributes(const ModuleAST *module,
                         StringPool &pool,
                         DiagnosticEngine &diag)
    {
        if (module == nullptr)
            return;

        // The one recognized attribute, interned once.
        const InternedString exportName = pool.intern("export");

        for (DeclAST *decl : module->decls)
        {
            if (decl == nullptr)
                continue;

            // Track which attribute names have been seen on this
            // declaration, to detect duplicates.
            std::vector<InternedString> seen;

            for (AttributeAST *attr : decl->attributes)
            {
                if (attr == nullptr)
                    continue;

                // ─── Unknown attribute ────────────────────────────────────
                if (attr->name != exportName)
                {
                    diag.error(DiagCode::Attr_Unknown, attr,
                               "unknown attribute '@",
                               pool.lookupView(attr->name), "'");
                    continue;
                }

                // ─── @export on an import ─────────────────────────────────
                if (decl->kind == ASTKind::ImportDecl)
                {
                    diag.error(DiagCode::Attr_ExportOnImport, attr,
                               "@export is not allowed on an import");
                    continue;
                }

                // ─── Duplicate @export ────────────────────────────────────
                bool duplicate = false;
                for (InternedString s : seen)
                {
                    if (s == attr->name)
                    {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate)
                {
                    diag.error(DiagCode::Attr_Duplicate, attr,
                               "duplicate attribute '@export'");
                    continue;
                }

                seen.push_back(attr->name);
            }
        }
    }

} // namespace lucid::sema