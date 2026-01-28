#include "optimizer/external_sort.h"
#include "common/definitions.h"
#include "storage/bptree/buffer/page_buffer_manager.h"
#include "storage/bptree/page/page.h"
#include "storage/bptree/page/page_format.h"
#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

using namespace page;

class ExternalSortTest : public ::testing::Test {
protected:
    static constexpr int number_frames = 10;
    PageBufferManager page_buffer_man{number_frames};

    // Helper: Create a page with integer data in slots
    Page CreatePageWithIntegers(const std::vector<int>& values)
    {
        page_id_t page_id = page_buffer_man.AllocatePage();
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        InitPage(page, PageType::Data);

        for (int value : values) {
            auto slot = AllocateSlot(page, sizeof(int));
            EXPECT_TRUE(slot.has_value());
            auto write_span = WriteRecord(page, slot.value());
            std::memcpy(write_span.data(), &value, sizeof(int));
        }

        return page;
    }

    // Helper: Read all non-deleted integers from page in slot order
    std::vector<int> ReadIntegersFromPage(Page& page)
    {
        std::vector<int> result;
        uint16_t capacity = GetPageCapacity(page);

        for (slot_id_t i = 0; i < capacity; i++) {
            if (!IsSlotDeleted(page, i)) {
                auto read_span = ReadRecord(page, i);
                int value;
                std::memcpy(&value, read_span.data(), sizeof(int));
                result.push_back(value);
            }
        }
        return result;
    }

    // Comparator: compares integers stored in PageSlice
    static bool IntComparator(PageSlice a, PageSlice b)
    {
        int val_a, val_b;
        std::memcpy(&val_a, a.data(), sizeof(int));
        std::memcpy(&val_b, b.data(), sizeof(int));
        return val_a < val_b;
    }
};

TEST_F(ExternalSortTest, SortEmptyPage)
{
    page_id_t page_id = page_buffer_man.AllocatePage();
    Page page = page_buffer_man.GetPage(page_id);
    std::lock_guard<Page> lg(page);
    InitPage(page, PageType::Data);

    RecordComparisonFunction comp = IntComparator;
    SortPageInPlace(page, comp);

    EXPECT_EQ(GetNumTuples(page), 0);
}

TEST_F(ExternalSortTest, SortSingleElement)
{
    Page page = CreatePageWithIntegers({42});
    std::lock_guard<Page> lg(page);

    RecordComparisonFunction comp = IntComparator;
    SortPageInPlace(page, comp);

    auto values = ReadIntegersFromPage(page);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values[0], 42);
}

TEST_F(ExternalSortTest, SortTwoElementsAscending)
{
    Page page = CreatePageWithIntegers({1, 2});
    std::lock_guard<Page> lg(page);

    RecordComparisonFunction comp = IntComparator;
    SortPageInPlace(page, comp);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
}

TEST_F(ExternalSortTest, SortTwoElementsDescending)
{
    Page page = CreatePageWithIntegers({2, 1});
    std::lock_guard<Page> lg(page);

    RecordComparisonFunction comp = IntComparator;
    SortPageInPlace(page, comp);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
}

TEST_F(ExternalSortTest, SortAlreadySorted)
{
    Page page = CreatePageWithIntegers({1, 2, 3, 4, 5});
    std::lock_guard<Page> lg(page);

    RecordComparisonFunction comp = IntComparator;
    SortPageInPlace(page, comp);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 5);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(values[i], i + 1);
    }
}

TEST_F(ExternalSortTest, SortReverseSorted)
{
    Page page = CreatePageWithIntegers({5, 4, 3, 2, 1});
    std::lock_guard<Page> lg(page);

    RecordComparisonFunction comp = IntComparator;
    SortPageInPlace(page, comp);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 5);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(values[i], i + 1);
    }
}

TEST_F(ExternalSortTest, SortRandomOrder)
{
    Page page = CreatePageWithIntegers({3, 1, 4, 1, 5, 9, 2, 6});
    std::lock_guard<Page> lg(page);

    RecordComparisonFunction comp = IntComparator;
    SortPageInPlace(page, comp);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 8);

    // Verify sorted: 1, 1, 2, 3, 4, 5, 6, 9
    std::vector<int> expected = {1, 1, 2, 3, 4, 5, 6, 9};
    EXPECT_EQ(values, expected);
}

TEST_F(ExternalSortTest, SortWithDuplicates)
{
    Page page = CreatePageWithIntegers({5, 2, 8, 2, 9, 5, 5});
    std::lock_guard<Page> lg(page);

    RecordComparisonFunction comp = IntComparator;
    SortPageInPlace(page, comp);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 7);

    std::vector<int> expected = {2, 2, 5, 5, 5, 8, 9};
    EXPECT_EQ(values, expected);
}

TEST_F(ExternalSortTest, SwapSlotsBasic)
{
    Page page = CreatePageWithIntegers({10, 20, 30});
    std::lock_guard<Page> lg(page);

    // Swap slots 0 and 2
    SwapSlots(page, 0, 2);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 30);
    EXPECT_EQ(values[1], 20);
    EXPECT_EQ(values[2], 10);
}

TEST_F(ExternalSortTest, SwapSlotsAdjacent)
{
    Page page = CreatePageWithIntegers({100, 200});
    std::lock_guard<Page> lg(page);

    SwapSlots(page, 0, 1);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], 200);
    EXPECT_EQ(values[1], 100);
}

TEST_F(ExternalSortTest, ShiftSlotsLeftNoDeleted)
{
    Page page = CreatePageWithIntegers({1, 2, 3, 4, 5});
    std::lock_guard<Page> lg(page);

    ShiftSlotsLeft(page);

    // Should remain unchanged
    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 5);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(values[i], i + 1);
    }
}

TEST_F(ExternalSortTest, ShiftSlotsLeftWithDeleted)
{
    page_id_t page_id = page_buffer_man.AllocatePage();
    Page page = page_buffer_man.GetPage(page_id);
    std::lock_guard<Page> lg(page);
    InitPage(page, PageType::Data);

    // Create slots with values 1, 2, 3, 4, 5
    std::vector<slot_id_t> slots;
    for (int i = 1; i <= 5; i++) {
        auto slot = AllocateSlot(page, sizeof(int));
        ASSERT_TRUE(slot.has_value());
        slots.push_back(slot.value());
        auto write_span = WriteRecord(page, slot.value());
        std::memcpy(write_span.data(), &i, sizeof(int));
    }

    // Delete slots 1 and 3 (values 2 and 4)
    DeleteSlot(page, slots[1]);
    DeleteSlot(page, slots[3]);

    EXPECT_EQ(GetNumTuples(page), 3); // 1, 3, 5 remain

    ShiftSlotsLeft(page);

    // After shift, non-deleted slots should be compacted to the left
    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 3);
    EXPECT_EQ(values[2], 5);
}

TEST_F(ExternalSortTest, ShiftSlotsLeftDeletedAtBeginning)
{
    page_id_t page_id = page_buffer_man.AllocatePage();
    Page page = page_buffer_man.GetPage(page_id);
    std::lock_guard<Page> lg(page);
    InitPage(page, PageType::Data);

    std::vector<slot_id_t> slots;
    for (int i = 1; i <= 4; i++) {
        auto slot = AllocateSlot(page, sizeof(int));
        ASSERT_TRUE(slot.has_value());
        slots.push_back(slot.value());
        auto write_span = WriteRecord(page, slot.value());
        std::memcpy(write_span.data(), &i, sizeof(int));
    }

    // Delete first two slots
    DeleteSlot(page, slots[0]);
    DeleteSlot(page, slots[1]);

    ShiftSlotsLeft(page);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], 3);
    EXPECT_EQ(values[1], 4);
}

TEST_F(ExternalSortTest, SortPageWithBounds)
{
    Page page = CreatePageWithIntegers({10, 5, 3, 8, 2, 9, 1});
    std::lock_guard<Page> lg(page);

    RecordComparisonFunction comp = IntComparator;

    // Sort only slots [2, 5) - values: 3, 8, 2
    SortPageInPlace(page, comp, 2, 5);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 7);

    // First two remain unchanged
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 5);

    // Middle section [2,5) should be sorted: 2, 3, 8
    EXPECT_EQ(values[2], 2);
    EXPECT_EQ(values[3], 3);
    EXPECT_EQ(values[4], 8);

    // Last two remain unchanged
    EXPECT_EQ(values[5], 9);
    EXPECT_EQ(values[6], 1);
}

TEST_F(ExternalSortTest, SortLargerDataset)
{
    // Create a larger unsorted dataset
    std::vector<int> input = {15, 3, 9, 1, 23, 7, 12, 5, 18, 11,
                              2, 19, 8, 14, 6, 17, 4, 13, 10, 20};

    Page page = CreatePageWithIntegers(input);
    std::lock_guard<Page> lg(page);

    RecordComparisonFunction comp = IntComparator;
    SortPageInPlace(page, comp);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 20);

    // Verify it's sorted
    for (size_t i = 1; i < values.size(); i++) {
        EXPECT_LE(values[i-1], values[i])
            << "Values not sorted at index " << i;
    }
}

TEST_F(ExternalSortTest, SortAllSameValues)
{
    Page page = CreatePageWithIntegers({7, 7, 7, 7, 7});
    std::lock_guard<Page> lg(page);

    RecordComparisonFunction comp = IntComparator;
    SortPageInPlace(page, comp);

    auto values = ReadIntegersFromPage(page);
    ASSERT_EQ(values.size(), 5);
    for (int val : values) {
        EXPECT_EQ(val, 7);
    }
}

TEST_F(ExternalSortTest, SortThreeElements)
{
    // Test various permutations of 3 elements
    std::vector<std::vector<int>> permutations = {
        {1, 2, 3}, {1, 3, 2}, {2, 1, 3},
        {2, 3, 1}, {3, 1, 2}, {3, 2, 1}
    };

    for (const auto& perm : permutations) {
        Page page = CreatePageWithIntegers(perm);
        std::lock_guard<Page> lg(page);

        RecordComparisonFunction comp = IntComparator;
        SortPageInPlace(page, comp);

        auto values = ReadIntegersFromPage(page);
        ASSERT_EQ(values.size(), 3);
        EXPECT_EQ(values[0], 1);
        EXPECT_EQ(values[1], 2);
        EXPECT_EQ(values[2], 3);
    }
}
