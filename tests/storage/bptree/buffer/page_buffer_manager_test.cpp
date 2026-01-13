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

    void SetUp() override
    {
        page_id = page_buffer_man.AllocatePage();
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        page.InitPage();
    }
};

TEST_F(PageBufferManagerTest, TestPageInit)
{
    Page page = page_buffer_man.GetPage(page_id);
    std::shared_lock<Page> sl(page);

    ASSERT_EQ(page.GetPageId(), page_id);
    ASSERT_EQ(page.GetNumSlots(), 0);
    ASSERT_GT(page.GetFreeSpaceSize(), 0);
}

TEST_F(PageBufferManagerTest, TestAllocateSlot)
{
    Page page = page_buffer_man.GetPage(page_id);
    std::lock_guard<Page> lg(page);

    EXPECT_EQ(page.GetPageId(), page_id);
    EXPECT_EQ(page.GetNumSlots(), 0);
    EXPECT_GT(page.GetFreeSpaceSize(), 0);

    constexpr int data_size = 4;
    auto slot = page.AllocateSlot(data_size);

    EXPECT_EQ(page.GetNumSlots(), 1);
}

TEST_F(PageBufferManagerTest, TestPageReadWrite)
{
    /* prepare some mock data */
    std::vector<char> data(64);
    std::iota(data.begin(), data.end(), 'a');

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        std::span<char> write_span(data);

        ASSERT_EQ(page.GetNumSlots(), 0);

        auto slot = page.AllocateSlot(write_span.size_bytes());

        ASSERT_EQ(page.GetNumSlots(), 1);
        ASSERT_GT(page.GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());
        slot_id = *slot;

        page.WriteSlot(slot_id, data);
    }

    /* read data */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::shared_lock<Page> sl(page);
        ASSERT_EQ(page.GetNumSlots(), 1);

        std::span<const char> read_span = page.ReadSlot(slot_id);
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
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        ASSERT_EQ(page.GetNumSlots(), 0);

        std::span<char> write_span(data);
        auto slot = page.AllocateSlot(write_span.size_bytes());

        ASSERT_EQ(page.GetNumSlots(), 1);
        ASSERT_GT(page.GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        page.DeleteSlot(slot_id);
        ASSERT_EQ(page.GetNumSlots(), 0);
    }
}

#ifndef NDEBUG
TEST_F(PageBufferManagerTest, TestReadDeletedSlot)
{
    constexpr int slot_size = 4;

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        ASSERT_EQ(page.GetNumSlots(), 0);

        auto slot = page.AllocateSlot(slot_size);

        ASSERT_EQ(page.GetNumSlots(), 1);
        ASSERT_GT(page.GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        page.DeleteSlot(slot_id);
        ASSERT_EQ(page.GetNumSlots(), 0);
    }

    /* attempt to read deleted slot */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::shared_lock<Page> sl(page);
        ASSERT_DEATH(page.ReadSlot(slot_id), "");
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
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        ASSERT_EQ(page.GetNumSlots(), 0);

        auto slot = page.AllocateSlot(sizeof(data));

        ASSERT_EQ(page.GetNumSlots(), 1);
        ASSERT_GT(page.GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        page.DeleteSlot(slot_id);
        ASSERT_EQ(page.GetNumSlots(), 0);
    }

    /* attempt to write to deleted slot */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        ASSERT_DEATH(page.WriteSlot(slot_id, { &data, sizeof(data) }), "");
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
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        for (int i = 0; i < num_slots; i++) {
            ASSERT_EQ(page.GetNumSlots(), i);

            auto slot = page.AllocateSlot(data_size);
            ASSERT_TRUE(slot.has_value());
            slots.push_back(slot.value());
        }
    }

    offset_t free_space_size;
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::shared_lock<Page> sl(page);
        free_space_size = page.GetFreeSpaceSize();
    }

    /* delete the slots from the page */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        for (slot_id_t slot_id : slots) {
            page.DeleteSlot(slot_id);
        }
        page.VacuumPage();
    }

    /* Should regain all of the space made from the previous allocations of
     * 10 slots of size 10. Note we NEVER reclaim slots. */
    constexpr int reclaimed_space = num_slots * data_size;
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::shared_lock<Page> sl(page);
        ASSERT_EQ(page.GetFreeSpaceSize() - num_slots * data_size, free_space_size);
    }
}

TEST_F(PageBufferManagerTest, TestVacuumPageNoReusableSpace)
{
    Page page = page_buffer_man.GetPage(page_id);
    std::lock_guard<Page> lg(page);

    /* allocate slots in page */
    std::vector<slot_id_t> slots;
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(page.GetNumSlots(), i);

        auto slot = page.AllocateSlot(4);
        ASSERT_TRUE(slot.has_value());
        slots.push_back(slot.value());
    }

    offset_t free_space_size = page.GetFreeSpaceSize();

    page.VacuumPage();

    /* vacuuming shouldn't have any effect here since there's no deleted pages */
    ASSERT_EQ(page.GetFreeSpaceSize(), free_space_size);
}

TEST_F(PageBufferManagerTest, TestVacuumPageMiddleInnerSlot)
{
    /* When deleting a slot all subsequent slots to the left (lower offset) in
     * the page should be shifted to the right (higher offset) to the end of
     * the page. This tests validates that all of the deleted space is
     * re-acquired. Specifically a slot not at the end or beginning in the
     * slot directory such that edge cases are roughly checked.
     */
    Page page = page_buffer_man.GetPage(page_id);
    std::lock_guard<Page> lg(page);

    /* allocate slots in page */
    constexpr int num_slots = 10;
    constexpr int data_size = 4;
    std::vector<slot_id_t> slots;
    for (int i = 0; i < num_slots; i++) {
        ASSERT_EQ(page.GetNumSlots(), i);

        auto slot = page.AllocateSlot(data_size);
        ASSERT_TRUE(slot.has_value());

        slots.push_back(slot.value());
    }

    offset_t free_space_size = page.GetFreeSpaceSize();

    /* delete slots in the center */
    constexpr int deleted_slots = 4;
    for (int i = 0; i < deleted_slots; i++) {
        page.DeleteSlot(slots[2 + i]);
    }

    ASSERT_EQ(page.GetNumSlots(), num_slots - deleted_slots);

    page.VacuumPage();

    std::size_t reclaimed_space = data_size * deleted_slots;
    ASSERT_EQ(page.GetFreeSpaceSize(), free_space_size + reclaimed_space);
}

TEST_F(PageBufferManagerTest, TestVacuumPageMiddleInnerSlotIntegrity)
{
    /* When slots are deleted and then a flush occurs the slots preceding
     * the deleted slot will be shifted over to re-acquire the deleted slot's
     * space. This test insures that the data stays intact after being
     * shifted by the vacuum operation. */
    Page page = page_buffer_man.GetPage(page_id);
    std::lock_guard<Page> lg(page);

    /* allocate slots in page */
    constexpr int num_slots = 3;
    constexpr int data_size = 4;
    const std::vector<char> slot1_data(data_size, 'a');
    const std::vector<char> slot3_data(data_size, 'b');

    auto slot1 = page.AllocateSlot(data_size);
    ASSERT_TRUE(slot1.has_value());
    page.WriteSlot(slot1.value(), slot1_data);

    auto slot2 = page.AllocateSlot(data_size);
    ASSERT_TRUE(slot2.has_value());

    auto slot3 = page.AllocateSlot(data_size);
    ASSERT_TRUE(slot3.has_value());
    page.WriteSlot(slot3.value(), slot3_data);

    EXPECT_EQ(page.GetNumSlots(), 3);

    offset_t free_space_size = page.GetFreeSpaceSize();

    /* delete slots in the center */
    page.DeleteSlot(slot2.value());
    EXPECT_EQ(page.GetNumSlots(), 2);

    /* clean up deleted slots */
    page.VacuumPage();

    std::size_t reclaimed_space = data_size;
    EXPECT_EQ(page.GetFreeSpaceSize(), free_space_size + reclaimed_space);

    /* check the integrity of the shifted slots post flush operation */
    auto slot1_read_span = page.ReadSlot(slot1.value());
    EXPECT_TRUE(std::equal(
        slot1_data.begin(), slot1_data.end(),
        slot1_read_span.begin(), slot1_read_span.end()));

    auto slot3_read_span = page.ReadSlot(slot3.value());
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
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        slot_id = *page.AllocateSlot(sizeof(data));
        page.WriteSlot(slot_id, { &data, 1 });
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
            std::shared_lock<Page> sl(page);
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
        Page page = page_buffer_man.GetPage(page_id);
        char starting_data = 'z';
        std::lock_guard<Page> lg(page);
        slot_id = *page.AllocateSlot(sizeof(starting_data));
        page.WriteSlot(slot_id, { &starting_data, 1 });
    }

    // TODO
    // Could do the reverse here too and check to see if there's any issue
    // if I have the reader page acquired first and then concurrently
    // try and get a writer page.
    std::thread writer_thread([&]() {
        // acquire exclusive access to page
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);

        // wait for main thread to try and acquire read page
        std::this_thread::sleep_for(ms(3));

        // write some new data
        page.WriteSlot(slot_id, { &data, 1 });
    });

    // wait for writer thread to acquire the write page
    std::this_thread::sleep_for(ms(2));

    Page page_read = page_buffer_man.GetPage(page_id);
    std::shared_lock<Page> sl(page_read);
    EXPECT_EQ(*page_read.ReadSlot(slot_id).begin(), data);

    if (writer_thread.joinable())
        writer_thread.join();
}
