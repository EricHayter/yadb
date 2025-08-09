#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <span>

template <typename T>
uint64_t crc64(const std::span<T> data)
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    assert((data.size_bytes() % 8 == 0) && "Span size must be divisible by 8");

    auto byte_span = std::as_bytes(data);
    uint64_t res = 0;
    for (size_t i = 0; i < byte_span.size_bytes(); i += 8) {
        uint64_t chunk;
        std::memcpy(&chunk, byte_span.data() + i, 8);
        res ^= chunk;
    }

    return res;
}

template <typename T, std::size_t N>
uint64_t crc64(const std::span<T, N> data)
{
    return crc64(data);
}
