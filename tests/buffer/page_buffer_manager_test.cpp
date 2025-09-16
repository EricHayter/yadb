#include <numeric>

#include "gtest/gtest.h"

#include "buffer/page_buffer_manager.h"
#include "storage/page/base_page.h"


TEST(PageBufferManagerTest, TestPageInit)
{
    int number_frames = 1;
    PageBufferManager page_buffer_man(number_frames);

    page_id_t page_id = page_buffer_man.NewPage();
    auto page = page_buffer_man.WaitReadPage(page_id);
    ASSERT_TRUE(page.has_value());

    ASSERT_EQ(page->GetPageId(), page_id);
    ASSERT_EQ(page->GetNumSlots(), 0);
    ASSERT_GT(page->GetFreeSpaceSize(), 0);
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
        auto page = page_buffer_man.WaitWritePage(page_id);
        ASSERT_TRUE(page.has_value());

        std::span<char> write_span(data);

        ASSERT_EQ(page->GetNumSlots(), 0);

        auto slot = page->AllocateSlot(write_span.size_bytes());

        ASSERT_EQ(page->GetNumSlots(), 1);

        ASSERT_GT(page->GetFreeSpaceSize(), 0);
        ASSERT_TRUE(slot.has_value());
        slot_id = *slot;
    }


    /* write to page */
    {
        auto page = page_buffer_man.WaitWritePage(page_id);
        ASSERT_TRUE(page.has_value());
        page->WriteSlot(slot_id, data);
    }

    /* read data */
    {
        auto page = page_buffer_man.WaitReadPage(page_id);
        ASSERT_TRUE(page.has_value());

        ASSERT_EQ(page->GetNumSlots(), 1);

        std::span<const char> read_span = page->ReadSlot(slot_id);
        ASSERT_TRUE(std::equal(read_span.begin(), read_span.end(), data.begin(), data.end()));
    }
}
