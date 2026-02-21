#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

using slot_id_t = uint16_t;
using offset_t = uint16_t;
using page_id_t = uint32_t;

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
    INTEGER,    // std::int32_t
    TEXT,       // std::string
};

template<DataType T>
struct TypeMap;

template<>
struct TypeMap<DataType::INTEGER> {
    using type = std::int32_t;
};

template<>
struct TypeMap<DataType::TEXT> {
    using type = std::string;
};

template<DataType T>
using type_for = typename TypeMap<T>::type;


template<typename T>
struct EnumMap;

template<>
struct EnumMap<std::int32_t> {
    static constexpr DataType value = DataType::INTEGER;
};

template<>
struct EnumMap<std::string> {
    static constexpr DataType value = DataType::TEXT;
};

template<typename T>
constexpr auto enum_for = EnumMap<T>::value;

using string_length_t = std::uint16_t;

// Convert DataType enum to string representation
std::string ToString(DataType dataType);
