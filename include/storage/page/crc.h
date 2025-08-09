#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <span>

/**
 * @brief 64-bit checksum function
 *
 *  To add error detection in case of corruption, pages will include a 64-bit
 *  field in each page header that includes a 64-bit checksum using a simple
 *  parity check. The checksum works by splitting the given span into 8-byte
 *  segments and finds their XOR. Error detection can then be done by
 *  performing the parity check again with the same data and the generated
 *  value from before. The second calculation should return 0 otherwise there
 *  was some sort of corruption.
 *
 * @param data a span of data to calculate the checksum for (size must be
 * divisible by 8 and the type must be trivially copyable).
 *
 * @returns the result of the 64-bit parity check
 */
template <typename T>
uint64_t checksum64(const std::span<T> data)
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
uint64_t checksum64(const std::span<T, N> data)
{
    return checksum64(data);
}
