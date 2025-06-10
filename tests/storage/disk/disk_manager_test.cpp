#include "common/type_definitions.h"
#include "storage/disk/disk_manager.h"
#include <gtest/gtest.h>
#include <filesystem>

/**
 * \brief RAII class for creating a temporary directory for creating files in
 */
class DiskManagerTest : public testing::Test {
    protected:
    std::filesystem::path temp_dir{"testing_temp"};

    DiskManagerTest()
    {
        if (not std::filesystem::exists(temp_dir))
        {
            std::filesystem::create_directory(temp_dir);
        }
    }

    ~DiskManagerTest()
    {
        std::filesystem::remove_all(temp_dir);
    }
};

/**
 * \brief Perform a simple read/write to verify that data integrity
 */
TEST_F(DiskManagerTest, TestSimpleWriteRead)
{
    std::filesystem::path temp_db_file(temp_dir / "db_file");
    std::array<char, PAGE_SIZE> page_data_write;
    page_data_write.fill('A');

    DiskManager disk_manager(temp_db_file);

    page_id_t page_id = disk_manager.AllocatePage();

    disk_manager.WritePage(page_id, page_data_write);

    std::array<char, PAGE_SIZE> page_data_read;
    disk_manager.ReadPage(page_id, page_data_read);

    EXPECT_EQ(page_data_read, page_data_write);

    // ensure that a proper amount of data is being allocated in the file
    EXPECT_EQ(std::filesystem::file_size(temp_db_file), PAGE_SIZE);
}

/**
 * \brief Test is free pages are reused and new allocations are not made
 */
TEST_F(DiskManagerTest, TestFreePage)
{
    std::filesystem::path temp_db_file(temp_dir / "db_file");
    std::array<char, PAGE_SIZE> page_data_write;
    page_data_write.fill('A');

    DiskManager disk_manager(temp_db_file, 1);
    page_id_t page_id = disk_manager.AllocatePage();

    disk_manager.WritePage(page_id, page_data_write);
    
    disk_manager.DeletePage(page_id);

    page_id_t new_page_id = disk_manager.AllocatePage();
    disk_manager.WritePage(new_page_id, page_data_write);
    
    // ensure that only one page was used (i.e. the free page was used)
    EXPECT_EQ(std::filesystem::file_size(temp_db_file), PAGE_SIZE);
}

/**
 * \brief Ensure that resizing is done in the case that no free pages are 
 * available
 */
TEST_F(DiskManagerTest, TestResizePage)
{
    std::filesystem::path temp_db_file(temp_dir / "db_file");
    std::array<char, PAGE_SIZE> page_data_write;
    page_data_write.fill('A');
    DiskManager disk_manager(temp_db_file, 1);

    for (int i = 0; i < 8; i++)
    {
        page_id_t page_id = disk_manager.AllocatePage();
        disk_manager.WritePage(page_id, page_data_write);    
    }
}
