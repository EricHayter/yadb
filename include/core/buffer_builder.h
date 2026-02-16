#pragma once

#include <string_view>
#include <span>
#include <cstring>

class BufferBuilder {
    public:
    // likely might make sense to have a constructor that takes in a list of
    // types or a size to preallocate space
    BufferBuilder() = default;
    ~BufferBuilder();

    BufferBuilder(const BufferBuilder&) = delete;
    BufferBuilder& operator=(const BufferBuilder&) = delete;

    BufferBuilder(BufferBuilder&& other) noexcept;
    BufferBuilder& operator=(BufferBuilder&& other) noexcept;

    void Push(std::integral auto data);
    void Push(std::string_view data);

    std::span<const std::byte> const Data() const { return { data_m, size_m }; };
    private:
    constexpr static size_t DEFAULT_ALLOC_SIZE = 4;
    constexpr static float GROWTH_RATE = 2.0f;

    void AllocateSpace();
    void AllocateSpace(std::size_t size);
    size_t size_m{ 0 };
    size_t capacity_m{ 0 };
    std::byte* data_m{ nullptr };
};

inline void BufferBuilder::Push(std::integral auto value) {
    if (size_m + sizeof(value) > capacity_m)
        AllocateSpace();
    std::memcpy(data_m + size_m, &value, sizeof(value));
    size_m += sizeof(value);
}
