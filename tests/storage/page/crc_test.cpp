#include "storage/page/crc.h"
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

TEST(CRCTest, CRCTest)
{
    std::array<uint64_t, 64> arr;
    arr[0] = 0x0ULL;
    arr[1] = 0xFFFFFFFFFFFFFFFFULL; // all ones
    arr[2] = 0xAAAAAAAAAAAAAAAAULL; // alternating 1010...
    arr[3] = 0x5555555555555555ULL; // alternating 0101...
    arr[4] = 0x8000000000000000ULL; // highest bit set
    arr[5] = 0x1ULL; // lowest bit set

    // fill the rest of the sequence with many different values
    uint64_t seed = 0x123456789ABCDEFULL;
    for (int i = 6; i < arr.size(); i++) {
        seed = seed * 6364136223846793005ULL + 1;
        arr[i] = seed;
    }

    // store the CRC in the empty slot meant to hold the CRC
    arr[0] = checksum64(std::span<uint64_t>(arr));

    // recomputing the CRC should yield 0
    EXPECT_EQ(checksum64(std::span<uint64_t>(arr)), 0x0ULL);

    // increment the CRC to make sure it isn't giving false positives.
    arr[0]++;
    EXPECT_NE(checksum64(std::span<uint64_t>(arr)), 0x0ULL);
}
