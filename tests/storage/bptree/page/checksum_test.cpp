#include "storage/slotted_page/checksum.h"
#include <gtest/gtest.h>
#include <string.h>
#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include "common/definitions.h"

TEST(CRCTest, CheckComplement)
{
    /* Create a page with some data */
    std::vector<PageData> data(PAGE_SIZE);
    MutFullPage page(data);
    std::generate(page.begin(), page.end(), [n = 'a']() mutable { return PageData{static_cast<unsigned char>(n++)}; });

    /* Null out an 8 byte word for where the checksum will be stored */
    std::fill(page.begin(), page.begin() + sizeof(uint64_t), PageData{0x00});

    /* Calculate the checksum and place it in the page */
    uint64_t checksum = checksum64(page);
    memcpy(page.data(), &checksum, sizeof(checksum));

    /* Recalculating the checksum should return 0 */
    ASSERT_EQ(checksum64(page), 0x00);
}

TEST(CRCTest, CheckPageAltered)
{
    /* Create a page with some data */
    std::vector<PageData> data(PAGE_SIZE);
    MutFullPage page(data);
    std::generate(page.begin(), page.end(), [n = 'a']() mutable { return PageData{static_cast<unsigned char>(n++)}; });

    uint64_t initial_checksum = checksum64(page);

    /* Alter the page (simulating corruption */
    std::fill(page.begin(), page.begin() + 42, PageData{'z'});
    uint64_t new_checksum = checksum64(page);

    ASSERT_NE(initial_checksum, new_checksum);
}
