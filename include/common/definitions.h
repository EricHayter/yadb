#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <string>

using slot_id_t = uint16_t;
using offset_t = uint16_t;
using page_id_t = uint32_t;

struct row_id_t {
    page_id_t page_id;
    slot_id_t slot_id;

    bool operator<=>(const row_id_t&) const = default;
};

/* Size of ALL pages in the database. Maximum allowable value of 65536 due
 * to the constraints on definitions of offset and slot_id types */
constexpr std::size_t PAGE_SIZE = 4096;
using PageData = std::byte;

/* Represents and entire page */
using MutFullPage = std::span<PageData, PAGE_SIZE>;
using FullPage = std::span<const PageData, PAGE_SIZE>;

/* Represents a section of a page (typically a record) */
using MutPageSlice = std::span<PageData>;
using PageSlice = std::span<const PageData>;

using TableValue = std::variant<int, std::string>;

using string_length_t = std::uint16_t;
