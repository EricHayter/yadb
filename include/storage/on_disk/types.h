#pragma once

#include <cstdint>
#include <span>
#include <functional>

// Type definitions for on-disk storage
using file_id_t = uint32_t;
using page_id_t = uint32_t;
using slot_id_t = uint16_t;
using offset_t = uint16_t;
using row_id_t = uint64_t;

// Page size constant
constexpr uint32_t PAGE_SIZE = 4096;

// Combined file and page ID
struct file_page_id_t {
    file_id_t file_id;
    page_id_t page_id;

    auto operator<=>(const file_page_id_t&) const = default;
};

// Hash function for file_page_id_t to enable use in unordered_map

namespace std {
template <>
struct hash<file_page_id_t> {
    std::size_t operator()(const file_page_id_t& id) const noexcept {
        // Combine hashes of file_id and page_id
        std::size_t h1 = std::hash<file_id_t>{}(id.file_id);
        std::size_t h2 = std::hash<page_id_t>{}(id.page_id);
        return h1 ^ (h2 << 1);
    }
};
}

/* Size of ALL pages in the database. Maximum allowable value of 65536 due
 * to the constraints on definitions of offset and slot_id types */
using PageData = std::byte;

/* Represents and entire page */
using MutFullPage = std::span<PageData, PAGE_SIZE>;
using FullPage = std::span<const PageData, PAGE_SIZE>;

/* Represents a section of a page (typically a record) */
using MutPageSlice = std::span<PageData>;
using PageSlice = std::span<const PageData>;

// Row ID conversion functions
inline row_id_t MakeRowId(page_id_t page_id, slot_id_t slot_id) {
    // Combine page_id (upper 32 bits) and slot_id (lower 16 bits)
    return (static_cast<row_id_t>(page_id) << 16) | slot_id;
}

inline page_id_t GetPageIdFromRowId(row_id_t row_id) {
    return static_cast<page_id_t>(row_id >> 16);
}

inline slot_id_t GetSlotIdFromRowId(row_id_t row_id) {
    return static_cast<slot_id_t>(row_id & 0xFFFF);
}
