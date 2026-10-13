/// @file sema/src/sema/Registry.cpp
///
/// @brief Implementation of computeRegistryFingerprint.
///
/// ─── The algorithm ────────────────────────────────────────────────────────
/// FNV-1a over a canonical, byte-level serialization of the registry's
/// contents. The serialization hashes:
///
///   - A version byte (currently 1).
///   - The count and contents of each list (phases, enums, handles,
///     node types).
///   - Every name, prefixed by its length (little-endian uint32_t).
///   - Every enum member's name and value.
///   - Every node type's name, kind, category, phase, result type, and
///     the names and types of its arguments.
///
/// The serialization is deterministic: the same registry always
/// produces the same bytes in the same order, and therefore the same
/// fingerprint. Different registries produce different byte streams
/// (with overwhelming probability) and therefore different
/// fingerprints.
///
/// ─── Portability ──────────────────────────────────────────────────────────
/// Integers are hashed little-endian, one byte at a time. The algorithm
/// is defined by constants and an explicit byte order, so a fingerprint
/// computed on one platform matches the fingerprint computed on any
/// other platform for the same registry.
///
/// ─── Why FNV-1a ───────────────────────────────────────────────────────────
/// The registry is small (tens to hundreds of entries). FNV-1a is fast
/// and has no dependency; the alternative (a cryptographic hash, a
/// checksum library) would add dependencies for negligible benefit.

#include "sema/Registry.hpp"
#include "sema/Primitives.hpp"
#include "core/SourceLocation.hpp"

#include <cstdint>
#include <string_view>
#include <unordered_set>

namespace lucid::sema
{

    namespace
    {

        // ─── FNV-1a constants ──────────────────────────────────────────────

        constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
        constexpr uint64_t kFnvPrime = 1099511628211ULL;

        // ─── Algorithm version ─────────────────────────────────────────────
        //
        // Bump this when the serialization changes (for example, a new
        // field is added to a node type). Old fingerprints become
        // invalid; that is the intent.

        constexpr uint32_t kFingerprintVersion = 1;

        // ─── The hash state and its operations ─────────────────────────────

        struct Hasher
        {
            uint64_t state = kFnvOffsetBasis;

            void byte(uint8_t b) noexcept
            {
                state ^= static_cast<uint64_t>(b);
                state *= kFnvPrime;
            }

            void u32(uint32_t v) noexcept
            {
                // Little-endian: low byte first.
                byte(static_cast<uint8_t>(v & 0xFF));
                byte(static_cast<uint8_t>((v >> 8) & 0xFF));
                byte(static_cast<uint8_t>((v >> 16) & 0xFF));
                byte(static_cast<uint8_t>((v >> 24) & 0xFF));
            }

            void u64(uint64_t v) noexcept
            {
                u32(static_cast<uint32_t>(v & 0xFFFFFFFFu));
                u32(static_cast<uint32_t>((v >> 32) & 0xFFFFFFFFu));
            }

            void i64(int64_t v) noexcept
            {
                u64(static_cast<uint64_t>(v));
            }

            void string(std::string_view s) noexcept
            {
                u32(static_cast<uint32_t>(s.size()));
                for (char c : s)
                {
                    byte(static_cast<uint8_t>(c));
                }
            }

            void typeId(const TypeId &t) noexcept
            {
                byte(static_cast<uint8_t>(t.kind));
                string(t.name);
            }
        };

        // ─── Section hashers ───────────────────────────────────────────────

        void hashPhases(Hasher &h, const Registry &registry)
        {
            h.u32(static_cast<uint32_t>(registry.phases.size()));
            for (const PhaseInfo &phase : registry.phases)
            {
                h.string(phase.name);
            }
        }

        void hashEnums(Hasher &h, const Registry &registry)
        {
            h.u32(static_cast<uint32_t>(registry.enums.size()));
            for (const EnumTypeInfo &enumInfo : registry.enums)
            {
                h.string(enumInfo.name);
                h.u32(static_cast<uint32_t>(enumInfo.members.size()));
                for (const EnumMemberInfo &member : enumInfo.members)
                {
                    h.string(member.name);
                    h.i64(member.value);
                }
            }
        }

        void hashHandles(Hasher &h, const Registry &registry)
        {
            h.u32(static_cast<uint32_t>(registry.handles.size()));
            for (const HandleTypeInfo &handle : registry.handles)
            {
                h.string(handle.name);
            }
        }

        void hashNodeTypes(Hasher &h, const Registry &registry)
        {
            h.u32(static_cast<uint32_t>(registry.nodeTypes.size()));
            for (const NodeTypeInfo &info : registry.nodeTypes)
            {
                h.string(info.name);
                h.byte(static_cast<uint8_t>(info.kind));
                h.string(info.category);
                h.u32(info.phase);

                h.u32(static_cast<uint32_t>(info.args.size()));
                for (const NodeArgInfo &arg : info.args)
                {
                    h.string(arg.name);
                    h.typeId(arg.type);
                }

                h.typeId(info.resultType);
            }
        }

        // ─── Struct Helpers ───────────────────────────────────────────────

        bool enumHasZeroMember(const EnumTypeInfo &info) noexcept
        {
            for (const EnumMemberInfo &m : info.members)
                if (m.value == 0)
                    return true;
            return false;
        }

        /// True if `type` names a type the registry declares.
        /// Primitive names are always considered declared.
        bool typeIsDeclared(const Registry &r, const TypeId &type) noexcept
        {
            switch (type.kind)
            {
            case TypeId::Kind::Invalid:
                return false;
            case TypeId::Kind::Primitive:
                return isCanonicalPrimitive(type.name);
            case TypeId::Kind::Enum:
                return r.findEnum(type.name) != nullptr;
            case TypeId::Kind::Handle:
                return r.findHandle(type.name) != nullptr;
            case TypeId::Kind::Struct:
                return r.findStruct(type.name) != nullptr;
            }
            return false;
        }

        /// DFS over the struct reference graph. Marks each visited
        /// struct; reports a diagnostic if a struct is re-entered
        /// before it is finished.
        bool visitStruct(const Registry &r,
                         const StructTypeInfo &s,
                         std::unordered_set<std::string_view> &visiting,
                         std::unordered_set<std::string_view> &visited,
                         lucid::diag::DiagnosticEngine &diag)
        {
            if (visited.count(s.name))
                return true;
            if (visiting.count(s.name))
            {
                diag.errorAt(lucid::diag::DiagCode::Registry_StructCycle,
                             SourceLocation{1, 1},
                             "struct '", s.name,
                             "' contains itself, directly or "
                             "transitively");
                return false;
            }

            visiting.insert(s.name);
            for (const StructFieldInfo &f : s.fields)
            {
                if (f.type.kind == TypeId::Kind::Struct)
                {
                    const StructTypeInfo *sub = r.findStruct(f.type.name);
                    if (sub == nullptr)
                        continue; // reported separately
                    if (!visitStruct(r, *sub, visiting, visited, diag))
                        return false;
                }
            }
            visiting.erase(s.name);
            visited.insert(s.name);
            return true;
        }

    } // namespace

    bool validateRegistry(const Registry &registry,
                          lucid::diag::DiagnosticEngine &diag)
    {
        bool ok = true;

        // ─── Every enum has a zero member ──────────────────────────────
        for (const EnumTypeInfo &info : registry.enums)
        {
            if (!enumHasZeroMember(info))
            {
                diag.errorAt(lucid::diag::DiagCode::Registry_EnumWithoutZero,
                             SourceLocation{1, 1},
                             "enum '", info.name,
                             "' has no member with value 0; a resource "
                             "field of this type cannot be "
                             "zero-initialized");
                ok = false;
            }
        }

        // ─── Every struct field names a declared type ──────────────────
        for (const StructTypeInfo &s : registry.structs)
        {
            for (const StructFieldInfo &f : s.fields)
            {
                if (!typeIsDeclared(registry, f.type))
                {
                    diag.errorAt(
                        lucid::diag::DiagCode::Registry_UnknownFieldType,
                        SourceLocation{1, 1},
                        "struct '", s.name, "' has field '", f.name,
                        "' of unknown type '", f.type.name, "'");
                    ok = false;
                }
            }
        }

        // ─── The struct reference graph is acyclic ─────────────────────
        std::unordered_set<std::string_view> visiting;
        std::unordered_set<std::string_view> visited;
        for (const StructTypeInfo &s : registry.structs)
        {
            if (!visitStruct(registry, s, visiting, visited, diag))
            {
                ok = false;
                break; // one cycle is enough
            }
        }

        return ok;
    }

    // ─── Public entry point ───────────────────────────────────────────────────

    uint64_t computeRegistryFingerprint(const Registry &registry)
    {
        Hasher h;

        // Version byte. Every future change to the serialization bumps
        // this; old fingerprints are invalidated cleanly.
        h.u32(kFingerprintVersion);

        // Each section in a fixed order.
        hashPhases(h, registry);
        hashEnums(h, registry);
        hashHandles(h, registry);
        hashNodeTypes(h, registry);

        return h.state;
    }

} // namespace lucid::sema