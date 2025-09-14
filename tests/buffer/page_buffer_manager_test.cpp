#include "buffer/page_buffer_manager.h"
#include "gtest/gtest.h"

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
