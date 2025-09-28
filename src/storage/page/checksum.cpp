#include "storage/page/checksum.h"

#include <cstring>

/*
 * Calculates a 64-bit checksum
 *
 * Uses a simple 64-bit parity check to generate the checksum for simplicity
 */
uint64_t checksum64(PageView page)
{
    static_assert(page.size_bytes() % 8 == 0);

    uint64_t res = 0;
    for (size_t i = 0; i < page.size_bytes(); i += 8) {
        uint64_t chunk;
        std::memcpy(&chunk, page.data() + i, sizeof(chunk));
        res ^= chunk;
    }

    return res;
}
