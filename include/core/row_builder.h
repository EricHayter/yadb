#pragma once

#include "common/definitions.h"
#include <cstring>
#include <span>
#include <string_view>
#include <type_traits>

class RowBuilder {
public:
    // likely might make sense to have a constructor that takes in a list of
    // types or a size to preallocate space
    RowBuilder() = default;
    ~RowBuilder();

    RowBuilder(const RowBuilder&) = delete;
    RowBuilder& operator=(const RowBuilder&) = delete;

    RowBuilder(RowBuilder&& other) noexcept;
    RowBuilder& operator=(RowBuilder&& other) noexcept;

    template <DataType T>
    void Push(auto data);

    std::span<const std::byte> Data() const { return { data_m, size_m }; };

private:
    constexpr static size_t DEFAULT_ALLOC_SIZE = 4;
    constexpr static float GROWTH_RATE = 2.0f;

    void AllocateSpace();
    void AllocateSpace(std::size_t size);
    size_t size_m { 0 };
    size_t capacity_m { 0 };
    std::byte* data_m { nullptr };
};

template <DataType T>
void RowBuilder::Push(auto data)
{
    if constexpr (T == DataType::INTEGER) {
        using IntegerType = type_for<DataType::INTEGER>;
        static_assert(std::is_convertible_v<decltype(data), IntegerType>,
            "Push<DataType::INTEGER> requires data convertible to integer type");
        IntegerType integer_data = static_cast<IntegerType>(data);
        std::size_t required_size = size_m + sizeof(integer_data);
        if (required_size > capacity_m)
            AllocateSpace(required_size);
        std::memcpy(data_m + size_m, &integer_data, sizeof(integer_data));
        size_m += sizeof(integer_data);
    } else if constexpr (T == DataType::TEXT) {
        static_assert(std::is_convertible_v<decltype(data), std::string_view>,
            "Push<DataType::TEXT> requires data convertible to std::string_view");
        std::string_view string_data = static_cast<std::string_view>(data);
        string_length_t string_length = string_data.size();
        std::size_t required_size = size_m + string_length + sizeof(string_length);
        if (capacity_m < required_size)
            AllocateSpace(required_size);

        // write string length first
        std::memcpy(data_m + size_m, &string_length, sizeof(string_length));
        size_m += sizeof(string_length);

        // write string data
        std::memcpy(data_m + size_m, string_data.data(), string_length);
        size_m += string_length;
    }
}
