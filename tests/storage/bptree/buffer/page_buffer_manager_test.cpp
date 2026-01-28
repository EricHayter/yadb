#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>
#include "common/definitions.h"
#include "gtest/gtest.h"
#include "storage/buffer_manager/page.h"
#include "storage/buffer_manager/page_buffer_manager.h"
#include "storage/page/page_format.h"

using namespace page;

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
        InitPage(page, PageType::Data);
    }
};

TEST_F(PageBufferManagerTest, TestPageInit)
{
    Page page = page_buffer_man.GetPage(page_id);
    std::shared_lock<Page> sl(page);

    ASSERT_EQ(page.GetPageId(), page_id);
    ASSERT_EQ(GetNumTuples(page), 0);
    ASSERT_GT(GetFreeSpaceSize(page), 0);
}

TEST_F(PageBufferManagerTest, TestAllocateSlot)
{
    Page page = page_buffer_man.GetPage(page_id);
    std::lock_guard<Page> lg(page);

    EXPECT_EQ(page.GetPageId(), page_id);
    EXPECT_EQ(GetNumTuples(page), 0);
    EXPECT_GT(GetFreeSpaceSize(page), 0);

    constexpr int data_size = 4;
    auto slot = AllocateSlot(page,data_size);

    EXPECT_EQ(GetNumTuples(page), 1);
}

TEST_F(PageBufferManagerTest, TestPageReadWrite)
{
    /* prepare some mock data */
    std::vector<PageData> data(64);
    std::generate(data.begin(), data.end(), [n = 'a']() mutable { return PageData{static_cast<unsigned char>(n++)}; });

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);

        ASSERT_EQ(GetNumTuples(page), 0);

        auto slot = AllocateSlot(page, data.size());

        ASSERT_EQ(GetNumTuples(page), 1);
        ASSERT_GT(GetFreeSpaceSize(page), 0);
        ASSERT_TRUE(slot.has_value());
        slot_id = *slot;

        auto write_span = WriteRecord(page, slot_id);
        std::memcpy(write_span.data(), data.data(), data.size());
    }

    /* read data */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::shared_lock<Page> sl(page);
        ASSERT_EQ(GetNumTuples(page), 1);

        PageSlice read_span = ReadRecord(page,slot_id);
        ASSERT_TRUE(std::equal(read_span.begin(), read_span.end(), data.begin(), data.end()));
    }
}

TEST_F(PageBufferManagerTest, TestDeleteSlot)
{
    /* prepare some mock data */
    std::vector<PageData> data(64);
    std::generate(data.begin(), data.end(), [n = 'a']() mutable { return PageData{static_cast<unsigned char>(n++)}; });

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        ASSERT_EQ(GetNumTuples(page), 0);

        auto slot = AllocateSlot(page, data.size());

        ASSERT_EQ(GetNumTuples(page), 1);
        ASSERT_GT(GetFreeSpaceSize(page), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        DeleteSlot(page,slot_id);
        ASSERT_EQ(GetNumTuples(page), 0);
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
        ASSERT_EQ(GetNumTuples(page), 0);

        auto slot = AllocateSlot(page,slot_size);

        ASSERT_EQ(GetNumTuples(page), 1);
        ASSERT_GT(GetFreeSpaceSize(page), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        DeleteSlot(page,slot_id);
        ASSERT_EQ(GetNumTuples(page), 0);
    }

    /* attempt to read deleted slot */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::shared_lock<Page> sl(page);
        ASSERT_DEATH(ReadRecord(page,slot_id), "");
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
        ASSERT_EQ(GetNumTuples(page), 0);

        auto slot = AllocateSlot(page,sizeof(data));

        ASSERT_EQ(GetNumTuples(page), 1);
        ASSERT_GT(GetFreeSpaceSize(page), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        DeleteSlot(page,slot_id);
        ASSERT_EQ(GetNumTuples(page), 0);
    }

    /* attempt to write to deleted slot */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        ASSERT_DEATH(WriteRecord(page, slot_id), "");
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
            ASSERT_EQ(GetNumTuples(page), i);

            auto slot = AllocateSlot(page,data_size);
            ASSERT_TRUE(slot.has_value());
            slots.push_back(slot.value());
        }
    }

    offset_t free_space_size;
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::shared_lock<Page> sl(page);
        free_space_size = GetFreeSpaceSize(page);
    }

    /* delete the slots from the page */
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::lock_guard<Page> lg(page);
        for (slot_id_t slot_id : slots) {
            DeleteSlot(page,slot_id);
        }
        VacuumPage(page);
    }

    /* Should regain all of the space made from the previous allocations of
     * 10 slots of size 10. Note we NEVER reclaim slots. */
    constexpr int reclaimed_space = num_slots * data_size;
    {
        Page page = page_buffer_man.GetPage(page_id);
        std::shared_lock<Page> sl(page);
        ASSERT_EQ(GetFreeSpaceSize(page) - num_slots * data_size, free_space_size);
    }
}

TEST_F(PageBufferManagerTest, TestVacuumPageNoReusableSpace)
{
    Page page = page_buffer_man.GetPage(page_id);
    std::lock_guard<Page> lg(page);

    /* allocate slots in page */
    std::vector<slot_id_t> slots;
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(GetNumTuples(page), i);

        auto slot = AllocateSlot(page,4);
        ASSERT_TRUE(slot.has_value());
        slots.push_back(slot.value());
    }

    offset_t free_space_size = GetFreeSpaceSize(page);

    VacuumPage(page);

    /* vacuuming shouldn't have any effect here since there's no deleted pages */
    ASSERT_EQ(GetFreeSpaceSize(page), free_space_size);
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
        ASSERT_EQ(GetNumTuples(page), i);

        auto slot = AllocateSlot(page,data_size);
        ASSERT_TRUE(slot.has_value());

        slots.push_back(slot.value());
    }

    offset_t free_space_size = GetFreeSpaceSize(page);

    /* delete slots in the center */
    constexpr int deleted_slots = 4;
    for (int i = 0; i < deleted_slots; i++) {
        DeleteSlot(page,slots[2 + i]);
    }

    ASSERT_EQ(GetNumTuples(page), num_slots - deleted_slots);

    VacuumPage(page);

    std::size_t reclaimed_space = data_size * deleted_slots;
    ASSERT_EQ(GetFreeSpaceSize(page), free_space_size + reclaimed_space);
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
    const std::vector<PageData> slot1_data(data_size, PageData{'a'});
    const std::vector<PageData> slot3_data(data_size, PageData{'b'});

    auto slot1 = AllocateSlot(page,data_size);
    ASSERT_TRUE(slot1.has_value());
    auto write_span1 = WriteRecord(page, slot1.value());
    std::memcpy(write_span1.data(), slot1_data.data(), slot1_data.size());

    auto slot2 = AllocateSlot(page,data_size);
    ASSERT_TRUE(slot2.has_value());

    auto slot3 = AllocateSlot(page,data_size);
    ASSERT_TRUE(slot3.has_value());
    auto write_span3 = WriteRecord(page, slot3.value());
    std::memcpy(write_span3.data(), slot3_data.data(), slot3_data.size());

    EXPECT_EQ(GetNumTuples(page), 3);

    offset_t free_space_size = GetFreeSpaceSize(page);

    /* delete slots in the center */
    DeleteSlot(page,slot2.value());
    EXPECT_EQ(GetNumTuples(page), 2);

    /* clean up deleted slots */
    VacuumPage(page);

    std::size_t reclaimed_space = data_size;
    EXPECT_EQ(GetFreeSpaceSize(page), free_space_size + reclaimed_space);

    /* check the integrity of the shifted slots post flush operation */
    auto slot1_read_span = ReadRecord(page,slot1.value());
    EXPECT_TRUE(std::equal(
        slot1_data.begin(), slot1_data.end(),
        slot1_read_span.begin(), slot1_read_span.end()));

    auto slot3_read_span = ReadRecord(page,slot3.value());
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
        slot_id = *AllocateSlot(page,sizeof(data));
        auto write_span = WriteRecord(page, slot_id);
        std::memcpy(write_span.data(), &data, sizeof(data));
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
            EXPECT_EQ(*ReadRecord(page,slot_id).begin(), PageData{static_cast<unsigned char>(data)});
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
        slot_id = *AllocateSlot(page,sizeof(starting_data));
        auto write_span = WriteRecord(page, slot_id);
        std::memcpy(write_span.data(), &starting_data, sizeof(starting_data));
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
        auto write_span = WriteRecord(page, slot_id);
        std::memcpy(write_span.data(), &data, sizeof(data));
    });

    // wait for writer thread to acquire the write page
    std::this_thread::sleep_for(ms(2));

    Page page_read = page_buffer_man.GetPage(page_id);
    std::shared_lock<Page> sl(page_read);
    EXPECT_EQ(*ReadRecord(page_read,slot_id).begin(), PageData{static_cast<unsigned char>(data)});

    if (writer_thread.joinable())
        writer_thread.join();
}
