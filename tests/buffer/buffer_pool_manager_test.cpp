#include "buffer/buffer_pool_manager.h"
#include "buffer/page_guard.h"
#include "common/type_definitions.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <filesystem>

/**
 * \brief RAII class for creating a temporary directory for creating files in
 */
class DiskManagerTest : public testing::Test {
protected:
    std::filesystem::path temp_dir { "testing_temp" };

    DiskManagerTest()
    {
        if (not std::filesystem::exists(temp_dir)) {
            std::filesystem::create_directory(temp_dir);
        }
    }

    ~DiskManagerTest()
    {
        std::filesystem::remove_all(temp_dir);
    }
};

TEST_F(DiskManagerTest, TestWaitWriteRead)
{
    std::filesystem::path temp_db_file(temp_dir / "db_file");
    BufferPoolManager buffer_pool_man(2, temp_db_file);
    page_id_t page_id = buffer_pool_man.NewPage();

    {
        std::vector<char> buff(PAGE_SIZE, 'B');
        MutPageView page_view(buff.begin(), PAGE_SIZE);
        WritePageGuard page_guard = buffer_pool_man.WaitWritePage(page_id);
        std::copy(page_view.begin(), page_view.end(), page_guard.GetData().begin());
    }

    {
        const std::vector<char> expected_buf(PAGE_SIZE, 'B');
        std::vector<char> buff(PAGE_SIZE, 0);
        MutPageView page_view(buff.begin(), PAGE_SIZE);
        ReadPageGuard page_guard = buffer_pool_man.WaitReadPage(page_id);
        PageView page_data = page_guard.GetData();
        std::copy(page_data.begin(), page_data.end(), buff.begin());
        EXPECT_EQ(expected_buf, buff);
    }
}

TEST_F(DiskManagerTest, TestTryWriteRead)
{
    std::filesystem::path temp_db_file(temp_dir / "db_file");
    BufferPoolManager buffer_pool_man(1, temp_db_file);
    page_id_t page_id = buffer_pool_man.NewPage();

    {
        std::vector<char> buff(PAGE_SIZE, 'B');
        MutPageView page_view(buff.begin(), PAGE_SIZE);
        auto page_guard_opt = buffer_pool_man.TryWritePage(page_id);
        ASSERT_TRUE(page_guard_opt.has_value());
        WritePageGuard page_guard = std::move(*page_guard_opt);
        std::copy(page_view.begin(), page_view.end(), page_guard.GetData().begin());
    }

    {
        const std::vector<char> expected_buf(PAGE_SIZE, 'B');
        std::vector<char> buff(PAGE_SIZE, 0);
        MutPageView page_view(buff.begin(), PAGE_SIZE);
        auto page_guard_opt = buffer_pool_man.TryReadPage(page_id);
        EXPECT_TRUE(page_guard_opt.has_value());
        ReadPageGuard page_guard = std::move(*page_guard_opt);
        PageView page_data = page_guard.GetData();
        std::copy(page_data.begin(), page_data.end(), buff.begin());
        EXPECT_EQ(expected_buf, buff);
    }
}
