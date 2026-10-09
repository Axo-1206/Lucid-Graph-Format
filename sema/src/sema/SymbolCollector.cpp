/// @file sema/src/sema/SymbolCollector.cpp
///
/// @brief Implementation of Pass 1's symbol collector.

#include "SymbolCollector.hpp"
#include "SymbolKindOf.hpp"

namespace lucid::sema
{

    void collectSymbols(const ModuleAST *module,
                        SymbolTable &table,
                        lucid::diag::DiagnosticEngine &diag)
    {
        if (module == nullptr)
        {
            return;
        }

        for (DeclAST *decl : module->decls)
        {
            if (decl == nullptr)
            {
                continue;
            }

            // Skip declarations that are not symbols. An UnknownAST
            // (from a parse error) has no name and no kind we can use.
            SymbolKind kind;
            if (!symbolKindOf(decl->kind, kind))
            {
                continue;
            }

            // Skip symbols with an invalid name. The parser produces a
            // valid name on a successful parse; a parse error can leave
            // the name invalid. Reporting a symbol with no name would
            // produce confusing diagnostics.
            if (!decl->name.isValid())
            {
                continue;
            }

            table.add(decl->name, kind, decl, diag);
        }
    }

} // namespace lucid::sema
