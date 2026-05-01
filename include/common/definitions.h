#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <variant>
#include <vector>

// Row ID is a simple 64-bit integer
using row_id_t = uint64_t;

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
