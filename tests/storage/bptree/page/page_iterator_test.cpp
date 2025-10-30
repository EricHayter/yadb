#include "storage/bptree/buffer/page_buffer_manager.h"
#include "storage/bptree/page/page.h"
#include "storage/bptree/page/page_iterator.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <shared_mutex>
#include <string>
#include <vector>

/**
 * \brief Test fixture for PageIterator tests
 */
class PageIteratorTest : public ::testing::Test {
protected:
    static constexpr int number_frames = 10;
    PageBufferManager page_buffer_man { number_frames };
    page_id_t page_id;
    std::optional<Page> page;

    void SetUp() override
    {
        page_id = page_buffer_man.AllocatePage();
        page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(*page);
        page->InitPage(PageType::Data);
    }

    // Helper to insert a slot with string data
    slot_id_t InsertSlot(const std::string& data)
    {
        auto slot = page->AllocateSlot(data.size());
        EXPECT_TRUE(slot.has_value());
        page->WriteSlot(*slot, std::span<const char>(data.data(), data.size()));
        return *slot;
    }

    // Helper to compare span with string
    bool SpanEquals(std::span<const char> span, const std::string& str)
    {
        if (span.size() != str.size()) {
            return false;
        }
        return std::equal(span.begin(), span.end(), str.begin());
    }
};

/**
 * \brief Test iterating over an empty page
 */
TEST_F(PageIteratorTest, TestEmptyPage)
{
    std::shared_lock<Page> sl(*page);

    auto it = page->begin();
    auto end = page->end();

    EXPECT_EQ(it, end) << "Begin should equal end for empty page";
}

/**
 * \brief Test iterating over a page with a single slot
 */
TEST_F(PageIteratorTest, TestSingleSlot)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("data1");
    }

    std::shared_lock<Page> sl(*page);

    int count = 0;
    for (slot_id_t slot_id : *page) {
        EXPECT_EQ(slot_id, 0);
        auto data = page->ReadSlot(slot_id);
        EXPECT_TRUE(SpanEquals(data, "data1"));
        ++count;
    }

    EXPECT_EQ(count, 1) << "Should iterate over exactly one slot";
}

/**
 * \brief Test iterating over a page with multiple slots
 */
TEST_F(PageIteratorTest, TestMultipleSlots)
{
    std::vector<std::string> test_data = { "first", "second", "third", "fourth", "fifth" };

    {
        std::lock_guard<Page> lg(*page);
        for (const auto& data : test_data) {
            InsertSlot(data);
        }
    }

    std::shared_lock<Page> sl(*page);

    int count = 0;
    for (slot_id_t slot_id : *page) {
        EXPECT_EQ(slot_id, count);
        auto data = page->ReadSlot(slot_id);
        EXPECT_TRUE(SpanEquals(data, test_data[count]));
        ++count;
    }

    EXPECT_EQ(count, test_data.size());
}

/**
 * \brief Test that iterator skips deleted slots
 */
TEST_F(PageIteratorTest, TestSkipDeletedSlots)
{
    std::vector<slot_id_t> slots;
    std::vector<std::string> expected_data = { "slot0", "slot2", "slot4" };

    {
        std::lock_guard<Page> lg(*page);
        slots.push_back(InsertSlot("slot0"));
        slots.push_back(InsertSlot("slot1"));
        slots.push_back(InsertSlot("slot2"));
        slots.push_back(InsertSlot("slot3"));
        slots.push_back(InsertSlot("slot4"));

        // Delete slots 1 and 3
        page->DeleteSlot(slots[1]);
        page->DeleteSlot(slots[3]);
    }

    std::shared_lock<Page> sl(*page);

    std::vector<slot_id_t> expected_slots = { 0, 2, 4 };
    int count = 0;
    for (slot_id_t slot_id : *page) {
        EXPECT_EQ(slot_id, expected_slots[count]);
        auto data = page->ReadSlot(slot_id);
        EXPECT_TRUE(SpanEquals(data, expected_data[count]));
        ++count;
    }

    EXPECT_EQ(count, 3) << "Should visit 3 non-deleted slots";
}

/**
 * \brief Test iterator pre-increment
 */
TEST_F(PageIteratorTest, TestPreIncrement)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("data1");
        InsertSlot("data2");
        InsertSlot("data3");
    }

    std::shared_lock<Page> sl(*page);

    auto it = page->begin();
    slot_id_t slot_id1 = *it;
    EXPECT_EQ(slot_id1, 0);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id1), "data1"));

    ++it;
    slot_id_t slot_id2 = *it;
    EXPECT_EQ(slot_id2, 1);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id2), "data2"));

    ++it;
    slot_id_t slot_id3 = *it;
    EXPECT_EQ(slot_id3, 2);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id3), "data3"));

    ++it;
    EXPECT_EQ(it, page->end());
}

/**
 * \brief Test iterator post-increment
 */
TEST_F(PageIteratorTest, TestPostIncrement)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("data1");
        InsertSlot("data2");
    }

    std::shared_lock<Page> sl(*page);

    auto it = page->begin();
    auto old_it = it++;

    slot_id_t old_slot_id = *old_it;
    EXPECT_EQ(old_slot_id, 0);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(old_slot_id), "data1"));

    slot_id_t new_slot_id = *it;
    EXPECT_EQ(new_slot_id, 1);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(new_slot_id), "data2"));
}

/**
 * \brief Test iterator pre-decrement
 */
TEST_F(PageIteratorTest, TestPreDecrement)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("data1");
        InsertSlot("data2");
        InsertSlot("data3");
    }

    std::shared_lock<Page> sl(*page);

    auto it = page->end();

    --it;
    slot_id_t slot_id3 = *it;
    EXPECT_EQ(slot_id3, 2);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id3), "data3"));

    --it;
    slot_id_t slot_id2 = *it;
    EXPECT_EQ(slot_id2, 1);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id2), "data2"));

    --it;
    slot_id_t slot_id1 = *it;
    EXPECT_EQ(slot_id1, 0);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id1), "data1"));

    EXPECT_EQ(it, page->begin());
}

/**
 * \brief Test iterator post-decrement
 */
TEST_F(PageIteratorTest, TestPostDecrement)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("data1");
        InsertSlot("data2");
    }

    std::shared_lock<Page> sl(*page);

    auto it = page->end();
    --it; // Move to last element (slot 1)

    auto old_it = it--;
    slot_id_t old_slot_id = *old_it;
    EXPECT_EQ(old_slot_id, 1);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(old_slot_id), "data2"));

    slot_id_t new_slot_id = *it;
    EXPECT_EQ(new_slot_id, 0);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(new_slot_id), "data1"));
}

/**
 * \brief Test bidirectional iteration (forward then backward)
 */
TEST_F(PageIteratorTest, TestBidirectionalIteration)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("data1");
        InsertSlot("data2");
        InsertSlot("data3");
    }

    std::shared_lock<Page> sl(*page);

    // Forward iteration
    auto it = page->begin();
    ++it; // Now at slot 1
    ++it; // Now at slot 2

    // Backward iteration
    --it; // Back to slot 1
    slot_id_t slot_id = *it;
    EXPECT_EQ(slot_id, 1);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id), "data2"));
}

/**
 * \brief Test iterator equality and inequality
 */
TEST_F(PageIteratorTest, TestEqualityComparison)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("data1");
        InsertSlot("data2");
    }

    std::shared_lock<Page> sl(*page);

    auto it1 = page->begin();
    auto it2 = page->begin();

    EXPECT_EQ(it1, it2);
    EXPECT_FALSE(it1 != it2);

    ++it1;
    EXPECT_NE(it1, it2);
    EXPECT_FALSE(it1 == it2);

    ++it2;
    EXPECT_EQ(it1, it2);
}

/**
 * \brief Test std::find_if with iterator
 */
TEST_F(PageIteratorTest, TestStdFindIf)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("apple");
        InsertSlot("banana");
        InsertSlot("cherry");
    }

    std::shared_lock<Page> sl(*page);

    auto it = std::find_if(page->begin(), page->end(),
        [this](slot_id_t slot_id) {
            return SpanEquals(page->ReadSlot(slot_id), "banana");
        });

    ASSERT_NE(it, page->end());
    slot_id_t slot_id = *it;
    EXPECT_EQ(slot_id, 1);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id), "banana"));
}

/**
 * \brief Test std::count_if with iterator
 */
TEST_F(PageIteratorTest, TestStdCountIf)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("short");
        InsertSlot("verylongstring");
        InsertSlot("tiny");
        InsertSlot("anotherlongone");
    }

    std::shared_lock<Page> sl(*page);

    // Count slots with data length > 5
    auto count = std::count_if(page->begin(), page->end(),
        [this](slot_id_t slot_id) {
            return page->ReadSlot(slot_id).size() > 5;
        });

    EXPECT_EQ(count, 2); // "verylongstring" and "anotherlongone"
}

/**
 * \brief Test std::distance with iterator
 */
TEST_F(PageIteratorTest, TestStdDistance)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("data1");
        InsertSlot("data2");
        InsertSlot("data3");
        InsertSlot("data4");
    }

    std::shared_lock<Page> sl(*page);

    auto distance = std::distance(page->begin(), page->end());
    EXPECT_EQ(distance, 4);
}

/**
 * \brief Test std::for_each with iterator
 */
TEST_F(PageIteratorTest, TestStdForEach)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("a");
        InsertSlot("bb");
        InsertSlot("ccc");
    }

    std::shared_lock<Page> sl(*page);

    size_t total_size = 0;
    std::for_each(page->begin(), page->end(),
        [this, &total_size](slot_id_t slot_id) {
            total_size += page->ReadSlot(slot_id).size();
        });

    EXPECT_EQ(total_size, 6); // 1 + 2 + 3
}

/**
 * \brief Test that iterator skips multiple consecutive deleted slots
 */
TEST_F(PageIteratorTest, TestSkipConsecutiveDeletedSlots)
{
    std::vector<slot_id_t> slots;
    std::vector<std::string> expected_data = { "data0", "data1", "data7", "data8", "data9" };

    {
        std::lock_guard<Page> lg(*page);
        for (int i = 0; i < 10; i++) {
            slots.push_back(InsertSlot("data" + std::to_string(i)));
        }

        // Delete slots 2, 3, 4, 5, 6
        for (int i = 2; i <= 6; i++) {
            page->DeleteSlot(slots[i]);
        }
    }

    std::shared_lock<Page> sl(*page);

    std::vector<slot_id_t> expected_slots = { 0, 1, 7, 8, 9 };
    int count = 0;
    for (slot_id_t slot_id : *page) {
        EXPECT_EQ(slot_id, expected_slots[count]);
        auto data = page->ReadSlot(slot_id);
        EXPECT_TRUE(SpanEquals(data, expected_data[count]));
        ++count;
    }

    EXPECT_EQ(count, 5);
}

/**
 * \brief Test decrementing past deleted slots
 */
TEST_F(PageIteratorTest, TestDecrementPastDeletedSlots)
{
    std::vector<slot_id_t> slots;

    {
        std::lock_guard<Page> lg(*page);
        slots.push_back(InsertSlot("slot0"));
        slots.push_back(InsertSlot("slot1"));
        slots.push_back(InsertSlot("slot2"));
        slots.push_back(InsertSlot("slot3"));
        slots.push_back(InsertSlot("slot4"));

        // Delete slot 2
        page->DeleteSlot(slots[2]);
    }

    std::shared_lock<Page> sl(*page);

    // Start at end and decrement
    auto it = page->end();

    --it; // Should be at slot 4
    slot_id_t slot_id4 = *it;
    EXPECT_EQ(slot_id4, 4);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id4), "slot4"));

    --it; // Should be at slot 3
    slot_id_t slot_id3 = *it;
    EXPECT_EQ(slot_id3, 3);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id3), "slot3"));

    --it; // Should skip slot 2 (deleted) and be at slot 1
    slot_id_t slot_id1 = *it;
    EXPECT_EQ(slot_id1, 1);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id1), "slot1"));

    --it; // Should be at slot 0
    slot_id_t slot_id0 = *it;
    EXPECT_EQ(slot_id0, 0);
    EXPECT_TRUE(SpanEquals(page->ReadSlot(slot_id0), "slot0"));

    EXPECT_EQ(it, page->begin());
}

/**
 * \brief Test iterator with all slots deleted
 */
TEST_F(PageIteratorTest, TestAllSlotsDeleted)
{
    std::vector<slot_id_t> slots;

    {
        std::lock_guard<Page> lg(*page);
        slots.push_back(InsertSlot("slot0"));
        slots.push_back(InsertSlot("slot1"));
        slots.push_back(InsertSlot("slot2"));

        // Delete all slots
        for (auto slot : slots) {
            page->DeleteSlot(slot);
        }
    }

    std::shared_lock<Page> sl(*page);

    // Iteration should be empty
    int count = 0;
    for (slot_id_t slot_id : *page) {
        (void)slot_id; // Suppress unused variable warning
        ++count;
    }

    EXPECT_EQ(count, 0) << "Should not iterate over any slots when all are deleted";
    EXPECT_EQ(page->begin(), page->end());
}

/**
 * \brief Test std::any_of with iterator
 */
TEST_F(PageIteratorTest, TestStdAnyOf)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("apple");
        InsertSlot("banana");
        InsertSlot("cherry");
    }

    std::shared_lock<Page> sl(*page);

    // Check if any slot contains "banana"
    bool has_banana = std::any_of(page->begin(), page->end(),
        [this](slot_id_t slot_id) {
            return SpanEquals(page->ReadSlot(slot_id), "banana");
        });

    EXPECT_TRUE(has_banana);

    // Check if any slot contains "grape"
    bool has_grape = std::any_of(page->begin(), page->end(),
        [this](slot_id_t slot_id) {
            return SpanEquals(page->ReadSlot(slot_id), "grape");
        });

    EXPECT_FALSE(has_grape);
}

/**
 * \brief Test std::all_of with iterator
 */
TEST_F(PageIteratorTest, TestStdAllOf)
{
    {
        std::lock_guard<Page> lg(*page);
        InsertSlot("data");
        InsertSlot("info");
        InsertSlot("test");
    }

    std::shared_lock<Page> sl(*page);

    // Check if all slots have size <= 4
    bool all_small = std::all_of(page->begin(), page->end(),
        [this](slot_id_t slot_id) {
            return page->ReadSlot(slot_id).size() <= 4;
        });

    EXPECT_TRUE(all_small);

    // Check if all slots have size > 5
    bool all_large = std::all_of(page->begin(), page->end(),
        [this](slot_id_t slot_id) {
            return page->ReadSlot(slot_id).size() > 5;
        });

    EXPECT_FALSE(all_large);
}
