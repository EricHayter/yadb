#include "common/type_definitions.h"
#include "storage/disk/disk_manager.h"
#include <filesystem>
#include <gtest/gtest.h>

/**
 * \brief RAII class for creating a temporary directory for creating files in
 */
class DiskManagerTest : public testing::Test {
protected:
    std::filesystem::path temp_dir { "testing_temp" };

    DiskManagerTest() = default;
    ~DiskManagerTest()
    {
        std::filesystem::remove_all(temp_dir);
    }
};

/**
 * \brief Test to make sure there is no errors with the created files when opening the disk manager again
 */
TEST_F(DiskManagerTest, TestCreateManagerTwice)
{
    {
        DiskManager disk_manager(temp_dir);
    }

    {
        DiskManager disk_manager(temp_dir);
    }
}

/**
 * \brief Perform a simple read/write to verify that data integrity
 */
TEST_F(DiskManagerTest, TestSimpleWriteRead)
{
    std::array<char, PAGE_SIZE> page_data_write;
    page_data_write.fill('A');

    DiskManager disk_manager(temp_dir);

    page_id_t page_id = disk_manager.AllocatePage();

    disk_manager.WritePage(page_id, page_data_write);

    std::array<char, PAGE_SIZE> page_data_read;
    disk_manager.ReadPage(page_id, page_data_read);

    EXPECT_EQ(page_data_read, page_data_write);
}

/**
 * \brief Test is free pages are reused and new allocations are not made
 */
TEST_F(DiskManagerTest, TestFreePage)
{
    std::array<char, PAGE_SIZE> page_data_write;
    page_data_write.fill('A');

    DiskManager disk_manager(temp_dir, 1);
    page_id_t page_id = disk_manager.AllocatePage();

    disk_manager.WritePage(page_id, page_data_write);

    disk_manager.DeletePage(page_id);

    page_id_t new_page_id = disk_manager.AllocatePage();
    disk_manager.WritePage(new_page_id, page_data_write);
}

/**
 * \brief Ensure that resizing is done in the case that no free pages are
 * available
 */
TEST_F(DiskManagerTest, TestResizePage)
{
    std::array<char, PAGE_SIZE> page_data_write;
    page_data_write.fill('A');
    DiskManager disk_manager(temp_dir, 1);

    for (int i = 0; i < 8; i++) {
        page_id_t page_id = disk_manager.AllocatePage();
        disk_manager.WritePage(page_id, page_data_write);
    }
}
