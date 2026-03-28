#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <variant>
#include <vector>

using slot_id_t = uint16_t;
using offset_t = uint16_t;
using page_id_t = uint32_t;
using file_id_t = uint32_t;

constexpr page_id_t NULL_PAGE_ID = -1;

struct file_page_id_t {
    file_id_t file_id;
    page_id_t page_id;

    auto operator<=>(const file_page_id_t&) const = default;
};

struct row_id_t {
    page_id_t page_id;
    slot_id_t slot_id;

    auto operator<=>(const row_id_t&) const = default;
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

/* list of all of the available types and enums */
enum class DataType : std::uint8_t {
    INTEGER, // std::int32_t
    TEXT, // std::string
};

template <DataType T>
struct TypeMap;

template <>
struct TypeMap<DataType::INTEGER> {
    using type = std::int32_t;
};

template <>
struct TypeMap<DataType::TEXT> {
    using type = std::string;
};

template <DataType T>
using type_for = typename TypeMap<T>::type;

// Variant type for SQL values - automatically derived from TypeMap
using Value = std::variant<
    type_for<DataType::INTEGER>,
    type_for<DataType::TEXT>>;

template <typename T>
struct EnumMap;

template <>
struct EnumMap<std::int32_t> {
    static constexpr DataType value = DataType::INTEGER;
};

template <>
struct EnumMap<std::string> {
    static constexpr DataType value = DataType::TEXT;
};

template <typename T>
constexpr auto enum_for = EnumMap<T>::value;

using string_length_t = std::uint16_t;

// Convert DataType enum to string representation
std::string ToString(DataType dataType);

struct RelationAttribute {
    std::string name;
    DataType type;
};

using Schema = std::vector<RelationAttribute>;

using Row = std::pair<row_id_t, std::span<const std::byte>>;

enum class TableError {
    INVALID_ROW_ID,
};

enum class TableType {
    InMemory,
    Disk
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
