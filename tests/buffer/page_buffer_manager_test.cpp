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

TEST(PageBufferManagerTest, TestReadDeletedSlot)
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

    /* attempt to read deleted slot */
    {
        Page page = page_buffer_man.ReadPage(page_id);
        ASSERT_THROW(page.ReadSlot(slot_id), std::runtime_error);
    }
}
