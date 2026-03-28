#include "common/definitions.h"
#include "storage/on_disk/disk/disk_manager.h"
#include <array>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

/**
 * \brief RAII class for creating a temporary directory for creating files in
 */
class DiskManagerTest : public testing::Test {
protected:
    std::filesystem::path temp_dir { "testing_temp" };

    DiskManagerTest()
    {
        std::filesystem::create_directories(temp_dir);
    }
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
        DiskManager disk_manager {};
    }

    {
        DiskManager disk_manager {};
    }
}

/**
 * \brief Perform a simple read/write to verify that data integrity
 */
TEST_F(DiskManagerTest, TestSimpleWriteRead)
{
    std::array<PageData, PAGE_SIZE> page_data_write;
    page_data_write.fill(PageData { 'A' });

    DiskManager disk_manager {};

    // Register a file first
    file_id_t file_id = disk_manager.RegisterFile(temp_dir / "test.db", 10);

    page_id_t page_id = disk_manager.AllocatePage(file_id);
    file_page_id_t fp_id{file_id, page_id};

    disk_manager.WritePage(fp_id, page_data_write);

    std::array<PageData, PAGE_SIZE> page_data_read;
    disk_manager.ReadPage(fp_id, page_data_read);

    EXPECT_EQ(page_data_read, page_data_write);
}

/**
 * \brief Test is free pages are reused and new allocations are not made
 */
TEST_F(DiskManagerTest, TestFreePage)
{
    std::array<PageData, PAGE_SIZE> page_data_write;
    page_data_write.fill(PageData { 'A' });

    DiskManager disk_manager {};

    // Register a file first
    file_id_t file_id = disk_manager.RegisterFile(temp_dir / "test2.db", 1);

    page_id_t page_id = disk_manager.AllocatePage(file_id);
    file_page_id_t fp_id{file_id, page_id};

    disk_manager.WritePage(fp_id, page_data_write);

    disk_manager.DeletePage(fp_id);

    page_id_t new_page_id = disk_manager.AllocatePage(file_id);
    file_page_id_t new_fp_id{file_id, new_page_id};
    disk_manager.WritePage(new_fp_id, page_data_write);
}

/**
 * \brief Ensure that resizing is done in the case that no free pages are
 * available
 */
TEST_F(DiskManagerTest, TestResizePage)
{
    std::array<PageData, PAGE_SIZE> page_data_write;
    page_data_write.fill(PageData { 'A' });
    DiskManager disk_manager {};

    // Register a file with initial capacity of 1
    file_id_t file_id = disk_manager.RegisterFile(temp_dir / "test3.db", 1);

    for (int i = 0; i < 8; i++) {
        page_id_t page_id = disk_manager.AllocatePage(file_id);
        file_page_id_t fp_id{file_id, page_id};
        disk_manager.WritePage(fp_id, page_data_write);
    }
}
