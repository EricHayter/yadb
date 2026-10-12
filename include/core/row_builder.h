#pragma once

#include "common/datatypes.h"
#include <span>
#include <stdexcept>

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

    void Push(const DataType auto& value);
    void Push(const Value& value);

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

void RowBuilder::Push(const DataType auto& value)
{
    std::size_t required_size = size_m + value.serialized_size();
    if (required_size > capacity_m)
        AllocateSpace(required_size);

    auto err = value.to_bytes({ data_m + size_m, capacity_m - size_m });
    if (err)
        throw std::runtime_error((*err)->what());
    size_m = required_size;
}
