/// @file StringPool.cpp
/// @brief Implementation of StringPool.

#include "core/memory/StringPool.hpp"

#include <algorithm>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

StringPool::StringPool() {
    // Reserve ID 0 for the empty / invalid handle. The vector's element 0
    // is a default-constructed string_view, which is what a caller gets
    // back from `lookupView(InternedString{})`.
    strings_.emplace_back();
}

// ─────────────────────────────────────────────────────────────────────────────
// Interning
// ─────────────────────────────────────────────────────────────────────────────

InternedString StringPool::intern(std::string_view s) {
    // Empty input is ID 0. Do not store it, do not look it up. ID 0 is
    // reserved, and no map entry points at it.
    if (s.empty()) return InternedString{};

    // Already interned? The map's key type is std::string_view, so this
    // does a string-hash lookup, but the bytes it hashes are the input's,
    // not a copy's. The map's stored keys point into the pool's blocks.
    auto it = internMap_.find(s);
    if (it != internMap_.end()) {
        return InternedString{it->second};
    }

    // Copy into a pool block, then store the copy's view in the map.
    // The order matters: the map key must be the pooled view, not the
    // caller's buffer.
    std::string_view pooled = allocateString(s);
    const uint32_t id = static_cast<uint32_t>(strings_.size());
    strings_.push_back(pooled);
    internMap_.emplace(pooled, id);
    return InternedString{id};
}

// ─────────────────────────────────────────────────────────────────────────────
// Lookup
// ─────────────────────────────────────────────────────────────────────────────

std::string StringPool::lookup(InternedString s) const {
    if (s.id >= strings_.size()) return {};
    return std::string{strings_[s.id]};
}

std::string_view StringPool::lookupView(InternedString s) const {
    if (s.id >= strings_.size()) return {};
    return strings_[s.id];
}

// ─────────────────────────────────────────────────────────────────────────────
// Bump allocation
// ─────────────────────────────────────────────────────────────────────────────

std::string_view StringPool::allocateString(std::string_view s) {
    const size_t len = s.size();

    // Need a new block if the current one cannot fit `len` bytes.
    if (!currentBlock_ || currentOffset_ + len > kBlockSize) {
        // A string larger than one block gets a block sized to fit it.
        const size_t blockSize = std::max(kBlockSize, len);
        blocks_.push_back(std::make_unique<char[]>(blockSize));
        currentBlock_  = blocks_.back().get();
        currentOffset_ = 0;
    }

    char* dst = currentBlock_ + currentOffset_;
    std::memcpy(dst, s.data(), len);
    currentOffset_ += len;

    return std::string_view{dst, len};
}