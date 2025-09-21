#include <numeric>

#include "gtest/gtest.h"

#include "buffer/page_buffer_manager.h"
#include "storage/page/base_page.h"


TEST(PageBufferManagerTest, TestPageInit)
{
    int number_frames = 1;
    PageBufferManager page_buffer_man(number_frames);

    page_id_t page_id = page_buffer_man.NewPage();
    Page page = page_buffer_man.ReadPage(page_id);

    ASSERT_EQ(page.GetPageId(), page_id);
    ASSERT_EQ(page.GetNumSlots(), 0);
    ASSERT_GT(page.GetFreeSpaceSize(), 0);
}

TEST(PageBufferManagerTest, TestPageReadWrite)
{
    /* prepare some mock data */
    std::vector<char> data(64);
    std::iota(data.begin(), data.end(), 'a');

    int number_frames = 1;
    PageBufferManager page_buffer_man(number_frames);

    page_id_t page_id = page_buffer_man.NewPage();

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        MutPage page = page_buffer_man.WritePage(page_id);

        std::span<char> write_span(data);

        ASSERT_EQ(page.GetNumSlots(), 0);

        auto slot = page.AllocateSlot(write_span.size_bytes());

        ASSERT_EQ(page.GetNumSlots(), 1);

        ASSERT_GT(page.GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());
        slot_id = *slot;
    }


    /* write to page */
    {
        MutPage page = page_buffer_man.WritePage(page_id);
        page.WriteSlot(slot_id, data);
    }

    /* read data */
    {
        Page page = page_buffer_man.ReadPage(page_id);
        ASSERT_EQ(page.GetNumSlots(), 1);

        std::span<const char> read_span = page.ReadSlot(slot_id);
        ASSERT_TRUE(std::equal(read_span.begin(), read_span.end(), data.begin(), data.end()));
    }
}

TEST(PageBufferManagerTest, TestDeleteSlot)
{
    /* prepare some mock data */
    std::vector<char> data(64);
    std::iota(data.begin(), data.end(), 'a');

    int number_frames = 1;
    PageBufferManager page_buffer_man(number_frames);

    page_id_t page_id = page_buffer_man.NewPage();

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        auto page = page_buffer_man.WritePage(page_id);
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
        MutPage page = page_buffer_man.WritePage(page_id);
        page.DeleteSlot(slot_id);
        ASSERT_EQ(page.GetNumSlots(), 0);
    }
}

#ifndef NDEBUG
TEST(PageBufferManagerTest, TestReadDeletedSlot)
{
    constexpr int slot_size = 4;
    constexpr int number_frames = 1;
    PageBufferManager page_buffer_man(number_frames);

    page_id_t page_id = page_buffer_man.NewPage();

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        auto page = page_buffer_man.WritePage(page_id);
        ASSERT_EQ(page.GetNumSlots(), 0);

        auto slot = page.AllocateSlot(slot_size);

        ASSERT_EQ(page.GetNumSlots(), 1);
        ASSERT_GT(page.GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        MutPage page = page_buffer_man.WritePage(page_id);
        page.DeleteSlot(slot_id);
        ASSERT_EQ(page.GetNumSlots(), 0);
    }

    /* attempt to read deleted slot */
    {
        Page page = page_buffer_man.ReadPage(page_id);
        ASSERT_DEATH(page.ReadSlot(slot_id), "");
    }
}
#endif

#ifndef NDEBUG
TEST(PageBufferManagerTest, TestWriteDeletedSlot)
{
    constexpr char data = 42;
    constexpr int number_frames = 1;
    PageBufferManager page_buffer_man(number_frames);

    page_id_t page_id = page_buffer_man.NewPage();

    /* allocate slot in page */
    slot_id_t slot_id;
    {
        auto page = page_buffer_man.WritePage(page_id);
        ASSERT_EQ(page.GetNumSlots(), 0);

        auto slot = page.AllocateSlot(sizeof(data));

        ASSERT_EQ(page.GetNumSlots(), 1);
        ASSERT_GT(page.GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());

        slot_id = *slot;
    }

    /* delete the slot */
    {
        MutPage page = page_buffer_man.WritePage(page_id);
        page.DeleteSlot(slot_id);
        ASSERT_EQ(page.GetNumSlots(), 0);
    }

    /* attempt to write to deleted slot */
    {
        MutPage page = page_buffer_man.WritePage(page_id);
        ASSERT_DEATH(page.WriteSlot(slot_id, { &data, sizeof(data) }), "");
    }
}
#endif

TEST(PageBufferManagerTest, TestFlushPage)
{
    constexpr int number_frames = 1;
    PageBufferManager page_buffer_man(number_frames);

    page_id_t page_id = page_buffer_man.NewPage();

    /* allocate slots in page */
    auto page = page_buffer_man.WritePage(page_id);

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

    /* delete the slots from the page */
    for (slot_id_t slot_id: slots) {
        page.DeleteSlot(slot_id);
    }

    page.VacuumPage();

    /* Should regain all of the space made from the previous allocations of
     * 10 slots of size 10. Note we NEVER reclaim slots. */
    ASSERT_EQ(page.GetFreeSpaceSize() - num_slots * data_size, free_space_size);
}

TEST(PageBufferManagerTest, TestFlushPageNoReusableSpace)
{
    constexpr int number_frames = 1;
    PageBufferManager page_buffer_man(number_frames);

    page_id_t page_id = page_buffer_man.NewPage();

    /* allocate slots in page */
    auto page = page_buffer_man.WritePage(page_id);

    std::vector<slot_id_t> slots;
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(page.GetNumSlots(), i);

        auto slot = page.AllocateSlot(4);
        ASSERT_TRUE(slot.has_value());
        slots.push_back(slot.value());
    }

    offset_t free_space_size = page.GetFreeSpaceSize();

    page.VacuumPage();

    /* vacuuming shouldn't have any effect here since there's no deleted pages
     * */
    ASSERT_EQ(page.GetFreeSpaceSize(), free_space_size);
}

TEST(PageBufferManagerTest, TestFlushPageDeletedInnerSlot)
{
    constexpr int number_frames = 1;
    PageBufferManager page_buffer_man(number_frames);

    page_id_t page_id = page_buffer_man.NewPage();
    auto page = page_buffer_man.WritePage(page_id);

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
