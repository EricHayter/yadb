#include <chrono>
#include <numeric>
#include <shared_mutex>
#include <thread>

#include "gtest/gtest.h"

#include "storage/bptree/buffer/page_buffer_manager.h"
#include "storage/bptree/page/page.h"

class PageBufferManagerTest : public ::testing::Test {
protected:
    static constexpr int number_frames = 1;
    PageBufferManager page_buffer_man { number_frames };
    page_id_t page_id;
    std::optional<Page> page;

    void SetUp() override
    {
        page_id = page_buffer_man.AllocatePage();
        page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Frame> lg(*page->GetFrame());
        page->InitPage(PageType::Data);
    }
};

TEST_F(PageBufferManagerTest, TestPageInit)
{
    std::shared_lock<Frame> sl(*page->GetFrame());

    ASSERT_EQ(page->GetPageId(), page_id);
    ASSERT_EQ(page->GetNumSlots(), 0);
    ASSERT_GT(page->GetFreeSpaceSize(), 0);
}

TEST_F(PageBufferManagerTest, TestAllocateSlot)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    EXPECT_EQ(page->GetPageId(), page_id);
    EXPECT_EQ(page->GetNumSlots(), 0);
    EXPECT_GT(page->GetFreeSpaceSize(), 0);

    constexpr int data_size = 4;
    auto slot = page->AllocateSlot(data_size);

    EXPECT_EQ(page->GetNumSlots(), 1);
}

TEST_F(PageBufferManagerTest, TestPageReadWrite)
{
    /* prepare some mock data */
    std::vector<char> data(64);
    std::iota(data.begin(), data.end(), 'a');

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        std::span<char> write_span(data);

        ASSERT_EQ(page->GetNumSlots(), 0);

        auto slot = page->AllocateSlot(write_span.size_bytes());

        ASSERT_EQ(page->GetNumSlots(), 1);
        ASSERT_GT(page->GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());
        slot_id = *slot;

        page->WriteSlot(slot_id, data);
    }

    /* read data */
    {
        std::shared_lock<Frame> sl(*page->GetFrame());
        ASSERT_EQ(page->GetNumSlots(), 1);

        std::span<const char> read_span = page->ReadSlot(slot_id);
        ASSERT_TRUE(std::equal(read_span.begin(), read_span.end(), data.begin(), data.end()));
    }
}

TEST_F(PageBufferManagerTest, TestDeleteSlot)
{
    /* prepare some mock data */
    std::vector<char> data(64);
    std::iota(data.begin(), data.end(), 'a');

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        ASSERT_EQ(page->GetNumSlots(), 0);

        std::span<char> write_span(data);
        auto slot = page->AllocateSlot(write_span.size_bytes());

        ASSERT_EQ(page->GetNumSlots(), 1);
        ASSERT_GT(page->GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        page->DeleteSlot(slot_id);
        ASSERT_EQ(page->GetNumSlots(), 0);
    }
}

#ifndef NDEBUG
TEST_F(PageBufferManagerTest, TestReadDeletedSlot)
{
    constexpr int slot_size = 4;

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        ASSERT_EQ(page->GetNumSlots(), 0);

        auto slot = page->AllocateSlot(slot_size);

        ASSERT_EQ(page->GetNumSlots(), 1);
        ASSERT_GT(page->GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        page->DeleteSlot(slot_id);
        ASSERT_EQ(page->GetNumSlots(), 0);
    }

    /* attempt to read deleted slot */
    {
        std::shared_lock<Frame> sl(*page->GetFrame());
        ASSERT_DEATH(page->ReadSlot(slot_id), "");
    }
}
#endif

#ifndef NDEBUG
TEST_F(PageBufferManagerTest, TestWriteDeletedSlot)
{
    constexpr char data = 42;

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        ASSERT_EQ(page->GetNumSlots(), 0);

        auto slot = page->AllocateSlot(sizeof(data));

        ASSERT_EQ(page->GetNumSlots(), 1);
        ASSERT_GT(page->GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        page->DeleteSlot(slot_id);
        ASSERT_EQ(page->GetNumSlots(), 0);
    }

    /* attempt to write to deleted slot */
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        ASSERT_DEATH(page->WriteSlot(slot_id, { &data, sizeof(data) }), "");
    }
}
#endif

TEST_F(PageBufferManagerTest, TestFlushPage)
{
    /* allocate slots in page */
    constexpr int num_slots = 10;
    constexpr int data_size = 4;
    std::vector<slot_id_t> slots;
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        for (int i = 0; i < num_slots; i++) {
            ASSERT_EQ(page->GetNumSlots(), i);

            auto slot = page->AllocateSlot(data_size);
            ASSERT_TRUE(slot.has_value());
            slots.push_back(slot.value());
        }
    }

    offset_t free_space_size;
    {
        std::shared_lock<Frame> sl(*page->GetFrame());
        free_space_size = page->GetFreeSpaceSize();
    }

    /* delete the slots from the page */
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        for (slot_id_t slot_id : slots) {
            page->DeleteSlot(slot_id);
        }
        page->VacuumPage();
    }

    /* Should regain all of the space made from the previous allocations of
     * 10 slots of size 10. Note we NEVER reclaim slots. */
    constexpr int reclaimed_space = num_slots * data_size;
    {
        std::shared_lock<Frame> sl(*page->GetFrame());
        ASSERT_EQ(page->GetFreeSpaceSize() - num_slots * data_size, free_space_size);
    }
}

TEST_F(PageBufferManagerTest, TestVacuumPageNoReusableSpace)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    /* allocate slots in page */
    std::vector<slot_id_t> slots;
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(page->GetNumSlots(), i);

        auto slot = page->AllocateSlot(4);
        ASSERT_TRUE(slot.has_value());
        slots.push_back(slot.value());
    }

    offset_t free_space_size = page->GetFreeSpaceSize();

    page->VacuumPage();

    /* vacuuming shouldn't have any effect here since there's no deleted pages */
    ASSERT_EQ(page->GetFreeSpaceSize(), free_space_size);
}

TEST_F(PageBufferManagerTest, TestVacuumPageMiddleInnerSlot)
{
    /* When deleting a slot all subsequent slots to the left (lower offset) in
     * the page should be shifted to the right (higher offset) to the end of
     * the page. This tests validates that all of the deleted space is
     * re-acquired. Specifically a slot not at the end or beginning in the
     * slot directory such that edge cases are roughly checked.
     */
    std::lock_guard<Frame> lg(*page->GetFrame());

    /* allocate slots in page */
    constexpr int num_slots = 10;
    constexpr int data_size = 4;
    std::vector<slot_id_t> slots;
    for (int i = 0; i < num_slots; i++) {
        ASSERT_EQ(page->GetNumSlots(), i);

        auto slot = page->AllocateSlot(data_size);
        ASSERT_TRUE(slot.has_value());

        slots.push_back(slot.value());
    }

    offset_t free_space_size = page->GetFreeSpaceSize();

    /* delete slots in the center */
    constexpr int deleted_slots = 4;
    for (int i = 0; i < deleted_slots; i++) {
        page->DeleteSlot(slots[2 + i]);
    }

    ASSERT_EQ(page->GetNumSlots(), num_slots - deleted_slots);

    page->VacuumPage();

    std::size_t reclaimed_space = data_size * deleted_slots;
    ASSERT_EQ(page->GetFreeSpaceSize(), free_space_size + reclaimed_space);
}

TEST_F(PageBufferManagerTest, TestVacuumPageMiddleInnerSlotIntegrity)
{
    /* When slots are deleted and then a flush occurs the slots preceding
     * the deleted slot will be shifted over to re-acquire the deleted slot's
     * space. This test insures that the data stays intact after being
     * shifted by the vacuum operation. */
    std::lock_guard<Frame> lg(*page->GetFrame());

    /* allocate slots in page */
    constexpr int num_slots = 3;
    constexpr int data_size = 4;
    const std::vector<char> slot1_data(data_size, 'a');
    const std::vector<char> slot3_data(data_size, 'b');

    auto slot1 = page->AllocateSlot(data_size);
    ASSERT_TRUE(slot1.has_value());
    page->WriteSlot(slot1.value(), slot1_data);

    auto slot2 = page->AllocateSlot(data_size);
    ASSERT_TRUE(slot2.has_value());

    auto slot3 = page->AllocateSlot(data_size);
    ASSERT_TRUE(slot3.has_value());
    page->WriteSlot(slot3.value(), slot3_data);

    EXPECT_EQ(page->GetNumSlots(), 3);

    offset_t free_space_size = page->GetFreeSpaceSize();

    /* delete slots in the center */
    page->DeleteSlot(slot2.value());
    EXPECT_EQ(page->GetNumSlots(), 2);

    /* clean up deleted slots */
    page->VacuumPage();

    std::size_t reclaimed_space = data_size;
    EXPECT_EQ(page->GetFreeSpaceSize(), free_space_size + reclaimed_space);

    /* check the integrity of the shifted slots post flush operation */
    auto slot1_read_span = page->ReadSlot(slot1.value());
    EXPECT_TRUE(std::equal(
        slot1_data.begin(), slot1_data.end(),
        slot1_read_span.begin(), slot1_read_span.end()));

    auto slot3_read_span = page->ReadSlot(slot3.value());
    EXPECT_TRUE(std::equal(
        slot3_data.begin(), slot3_data.end(),
        slot3_read_span.begin(), slot3_read_span.end()));
}

TEST_F(PageBufferManagerTest, MultipleConccurentReaders)
{
    using ms = std::chrono::milliseconds;

    constexpr char data = 'a';
    slot_id_t slot_id;

    // write some data to the page
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        slot_id = *page->AllocateSlot(sizeof(data));
        page->WriteSlot(slot_id, { &data, 1 });
    }

    // I expect this to run in parallel i.e. this loop and the joins should end
    // in around 3 ms + some extra time for thread creation. It should NOT take
    // equal or more than 24 ms.
    auto start_time = std::chrono::steady_clock::now();

    int num_readers = 8;
    std::vector<std::thread> threads(num_readers);
    for (int i = 0; i < num_readers; i++) {
        threads.push_back(std::thread([&]() {
            std::this_thread::sleep_for(ms(3));
            Page page = page_buffer_man.GetPage(page_id);
            std::shared_lock<Frame> sl(*page.GetFrame());
            EXPECT_EQ(*page.ReadSlot(slot_id).begin(), data);
        }));
    }

    for (auto& thread : threads)
        if (thread.joinable())
            thread.join();

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<ms>(end_time - start_time).count();

    ASSERT_LT(duration, 24);
}

TEST_F(PageBufferManagerTest, WriterReaderMutualExclusive)
{
    using ms = std::chrono::milliseconds;

    constexpr char data = 'a';
    slot_id_t slot_id;

    // write some data to the page
    {
        char starting_data = 'z';
        std::lock_guard<Frame> lg(*page->GetFrame());
        slot_id = *page->AllocateSlot(sizeof(starting_data));
        page->WriteSlot(slot_id, { &starting_data, 1 });
    }

    // TODO
    // Could do the reverse here too and check to see if there's any issue
    // if I have the reader page acquired first and then concurrently
    // try and get a writer page.
    std::thread writer_thread([&]() {
        // acquire exclusive access to page
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Frame> lg(*page.GetFrame());

        // wait for main thread to try and acquire read page
        std::this_thread::sleep_for(ms(3));

        // write some new data
        page.WriteSlot(slot_id, { &data, 1 });
    });

    // wait for writer thread to acquire the write page
    std::this_thread::sleep_for(ms(2));

    Page page_read = page_buffer_man.GetPage(page_id);
    std::shared_lock<Frame> sl(*page_read.GetFrame());
    EXPECT_EQ(*page_read.ReadSlot(slot_id).begin(), data);

    if (writer_thread.joinable())
        writer_thread.join();
}

// ============================================================================
// ResizeSlot Tests
// ============================================================================

// Test shrinking a slot (new_size < old_size)
TEST_F(PageBufferManagerTest, ResizeSlotShrink)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    // Allocate a slot with initial data
    constexpr int initial_size = 64;
    std::vector<char> initial_data(initial_size);
    std::iota(initial_data.begin(), initial_data.end(), 'a');

    auto slot = page->AllocateSlot(initial_size);
    ASSERT_TRUE(slot.has_value());
    slot_id_t slot_id = *slot;

    page->WriteSlot(slot_id, initial_data);

    // Verify initial data
    auto read_span = page->ReadSlot(slot_id);
    ASSERT_EQ(read_span.size(), initial_size);
    EXPECT_TRUE(std::equal(read_span.begin(), read_span.end(),
        initial_data.begin(), initial_data.end()));

    // Shrink the slot
    constexpr int new_size = 32;
    bool success = page->ResizeSlot(slot_id, new_size);
    ASSERT_TRUE(success);

    // Verify the slot was resized
    read_span = page->ReadSlot(slot_id);
    EXPECT_EQ(read_span.size(), new_size);

    // Verify that the tail portion of the data is preserved
    // (offset moves forward, so we keep the last new_size bytes)
    EXPECT_TRUE(std::equal(read_span.begin(), read_span.end(),
        initial_data.begin() + (initial_size - new_size), initial_data.end()));
}

// Test growing a slot (new_size > old_size) with sufficient space
TEST_F(PageBufferManagerTest, ResizeSlotGrowWithSufficientSpace)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    // Allocate a small slot
    constexpr int initial_size = 32;
    std::vector<char> initial_data(initial_size, 'x');

    auto slot = page->AllocateSlot(initial_size);
    ASSERT_TRUE(slot.has_value());
    slot_id_t slot_id = *slot;

    page->WriteSlot(slot_id, initial_data);

    offset_t free_space_before = page->GetFreeSpaceSize();

    // Grow the slot
    constexpr int new_size = 64;
    bool success = page->ResizeSlot(slot_id, new_size);
    ASSERT_TRUE(success);

    // Verify the slot was resized
    auto read_span = page->ReadSlot(slot_id);
    EXPECT_EQ(read_span.size(), new_size);

    // Verify free space decreased by the new allocation
    offset_t free_space_after = page->GetFreeSpaceSize();
    EXPECT_EQ(free_space_after, free_space_before - new_size);

    // Note: We don't verify data contents because the documentation
    // warns that ResizeSlot may invalidate the original data
}

// Test growing a slot when there's insufficient space
TEST_F(PageBufferManagerTest, ResizeSlotGrowInsufficientSpace)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    // Fill the page almost completely
    offset_t available_space = page->GetFreeSpaceSize();
    constexpr int slot_directory_overhead = 8; // approximate
    int large_allocation = available_space - slot_directory_overhead - 64;

    auto large_slot = page->AllocateSlot(large_allocation);
    ASSERT_TRUE(large_slot.has_value());

    // Now allocate a small slot
    constexpr int initial_size = 32;
    auto small_slot = page->AllocateSlot(initial_size);
    ASSERT_TRUE(small_slot.has_value());
    slot_id_t slot_id = *small_slot;

    std::vector<char> data(initial_size, 'y');
    page->WriteSlot(slot_id, data);

    // Try to grow beyond available space
    int huge_size = page->GetFreeSpaceSize() + 100;
    bool success = page->ResizeSlot(slot_id, huge_size);
    EXPECT_FALSE(success);

    // Verify the slot remains unchanged
    auto read_span = page->ReadSlot(slot_id);
    EXPECT_EQ(read_span.size(), initial_size);
}

// Test resizing to the same size (should succeed trivially)
TEST_F(PageBufferManagerTest, ResizeSlotSameSize)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    constexpr int size = 64;
    std::vector<char> data(size);
    std::iota(data.begin(), data.end(), 'a');

    auto slot = page->AllocateSlot(size);
    ASSERT_TRUE(slot.has_value());
    slot_id_t slot_id = *slot;

    page->WriteSlot(slot_id, data);

    offset_t free_space_before = page->GetFreeSpaceSize();

    // Resize to same size
    bool success = page->ResizeSlot(slot_id, size);
    ASSERT_TRUE(success);

    // Verify nothing changed
    auto read_span = page->ReadSlot(slot_id);
    EXPECT_EQ(read_span.size(), size);
    EXPECT_EQ(page->GetFreeSpaceSize(), free_space_before);

    // Data should be preserved
    EXPECT_TRUE(std::equal(read_span.begin(), read_span.end(),
        data.begin(), data.end()));
}

// Test shrinking to zero size
TEST_F(PageBufferManagerTest, ResizeSlotShrinkToZero)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    constexpr int initial_size = 64;
    auto slot = page->AllocateSlot(initial_size);
    ASSERT_TRUE(slot.has_value());
    slot_id_t slot_id = *slot;

    // Shrink to zero
    bool success = page->ResizeSlot(slot_id, 0);
    ASSERT_TRUE(success);

    // Verify the slot has zero size
    auto read_span = page->ReadSlot(slot_id);
    EXPECT_EQ(read_span.size(), 0);
}

// Test growing from zero size
TEST_F(PageBufferManagerTest, ResizeSlotGrowFromZero)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    // Allocate a zero-size slot
    auto slot = page->AllocateSlot(0);
    ASSERT_TRUE(slot.has_value());
    slot_id_t slot_id = *slot;

    // Grow to non-zero size
    constexpr int new_size = 32;
    bool success = page->ResizeSlot(slot_id, new_size);
    ASSERT_TRUE(success);

    // Verify the slot was resized
    auto read_span = page->ReadSlot(slot_id);
    EXPECT_EQ(read_span.size(), new_size);
}

// Test multiple resizes on the same slot
TEST_F(PageBufferManagerTest, ResizeSlotMultipleTimes)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    constexpr int initial_size = 64;
    std::vector<char> data(initial_size);
    std::iota(data.begin(), data.end(), 'a');

    auto slot = page->AllocateSlot(initial_size);
    ASSERT_TRUE(slot.has_value());
    slot_id_t slot_id = *slot;

    page->WriteSlot(slot_id, data);

    // First resize: shrink
    bool success = page->ResizeSlot(slot_id, 32);
    ASSERT_TRUE(success);
    EXPECT_EQ(page->ReadSlot(slot_id).size(), 32);

    // Second resize: shrink more
    success = page->ResizeSlot(slot_id, 16);
    ASSERT_TRUE(success);
    EXPECT_EQ(page->ReadSlot(slot_id).size(), 16);

    // Third resize: grow (will allocate new buffer)
    success = page->ResizeSlot(slot_id, 48);
    ASSERT_TRUE(success);
    EXPECT_EQ(page->ReadSlot(slot_id).size(), 48);

    // Fourth resize: shrink again
    success = page->ResizeSlot(slot_id, 24);
    ASSERT_TRUE(success);
    EXPECT_EQ(page->ReadSlot(slot_id).size(), 24);
}

// Test resizing with multiple slots on the page
TEST_F(PageBufferManagerTest, ResizeSlotWithMultipleSlots)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    // Create three slots
    constexpr int slot_size = 64;
    std::vector<char> data1(slot_size, 'a');
    std::vector<char> data2(slot_size, 'b');
    std::vector<char> data3(slot_size, 'c');

    auto slot1 = page->AllocateSlot(slot_size);
    auto slot2 = page->AllocateSlot(slot_size);
    auto slot3 = page->AllocateSlot(slot_size);

    ASSERT_TRUE(slot1.has_value() && slot2.has_value() && slot3.has_value());

    page->WriteSlot(*slot1, data1);
    page->WriteSlot(*slot2, data2);
    page->WriteSlot(*slot3, data3);

    // Resize the middle slot (shrink)
    bool success = page->ResizeSlot(*slot2, 32);
    ASSERT_TRUE(success);

    // Verify slot2 was resized
    EXPECT_EQ(page->ReadSlot(*slot2).size(), 32);

    // Verify other slots remain unchanged
    EXPECT_EQ(page->ReadSlot(*slot1).size(), slot_size);
    EXPECT_EQ(page->ReadSlot(*slot3).size(), slot_size);

    // Verify data integrity of unchanged slots
    auto read1 = page->ReadSlot(*slot1);
    auto read3 = page->ReadSlot(*slot3);
    EXPECT_TRUE(std::equal(read1.begin(), read1.end(), data1.begin(), data1.end()));
    EXPECT_TRUE(std::equal(read3.begin(), read3.end(), data3.begin(), data3.end()));
}

#ifndef NDEBUG
// Test resizing a deleted slot (should fail assertion in debug mode)
TEST_F(PageBufferManagerTest, ResizeSlotDeleted)
{
    constexpr int size = 64;
    slot_id_t slot_id;

    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        auto slot = page->AllocateSlot(size);
        ASSERT_TRUE(slot.has_value());
        slot_id = *slot;
        page->DeleteSlot(slot_id);
    }

    // Attempt to resize deleted slot
    {
        std::lock_guard<Frame> lg(*page->GetFrame());
        ASSERT_DEATH(page->ResizeSlot(slot_id, 32), "");
    }
}
#endif

// Test edge case: grow slot to exact remaining free space
TEST_F(PageBufferManagerTest, ResizeSlotGrowToExactFreeSpace)
{
    std::lock_guard<Frame> lg(*page->GetFrame());

    // Allocate a small slot
    constexpr int initial_size = 32;
    auto slot = page->AllocateSlot(initial_size);
    ASSERT_TRUE(slot.has_value());
    slot_id_t slot_id = *slot;

    std::vector<char> data(initial_size, 'z');
    page->WriteSlot(slot_id, data);

    // Get exact free space available
    offset_t free_space = page->GetFreeSpaceSize();

    // Try to grow to exactly the free space available
    bool success = page->ResizeSlot(slot_id, free_space);

    // This should succeed as we have exactly enough space
    EXPECT_TRUE(success);
    EXPECT_EQ(page->ReadSlot(slot_id).size(), free_space);
}
