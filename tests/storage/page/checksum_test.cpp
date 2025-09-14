#include "storage/page/checksum.h"
#include "storage/page/page.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>
#include <numeric>

TEST(CRCTest, CheckComplement)
{
    /* Create a page with some data */
    std::vector<char> data(PAGE_SIZE);
    MutPageView page(data);
    std::iota(page.begin(), page.end(), 'a');

    /* Null out an 8 byte word for where the checksum will be stored */
    std::fill(page.begin(), page.begin() + sizeof(uint64_t), 0x00);

    /* Calculate the checksum and place it in the page */
    uint64_t checksum = checksum64(page);
    memcpy(page.data(), &checksum, sizeof(checksum));

    /* Recalculating the checksum should return 0 */
    ASSERT_EQ(checksum64(page), 0x00);
}

TEST(CRCTest, CheckPageAltered)
{
    /* Create a page with some data */
    std::vector<char> data(PAGE_SIZE);
    MutPageView page(data);
    std::iota(page.begin(), page.end(), 'a');

    uint64_t initial_checksum = checksum64(page);

    /* Alter the page (simulating corruption */
    std::fill(page.begin(), page.begin() + 42, 'z');
    uint64_t new_checksum = checksum64(page);

    ASSERT_NE(initial_checksum, new_checksum);
}
