#include "storage/on_disk/disk/disk_manager.h"
#include "storage/on_disk/disk/disk_manager_error.h"
#include "storage/on_disk/types.h"
#include <array>
#include <filesystem>
#include <gtest/gtest.h>

static constexpr file_id_t TEST_FILE_ID = 99999;

static std::filesystem::path TestFilePath()
{
    return std::to_string(TEST_FILE_ID) + ".yadb";
}

class DiskManagerTest : public testing::Test {
protected:
    DiskManager disk_manager;

    void SetUp() override
    {
        std::filesystem::remove(TestFilePath());
    }

    void TearDown() override
    {
        std::filesystem::remove(TestFilePath());
    }
};

TEST_F(DiskManagerTest, CreateFileSucceeds)
{
    auto err = disk_manager.CreateFile(TEST_FILE_ID);
    EXPECT_FALSE(err.has_value());
    EXPECT_TRUE(std::filesystem::exists(TestFilePath()));
}

TEST_F(DiskManagerTest, CreateDuplicateFileReturnsError)
{
    disk_manager.CreateFile(TEST_FILE_ID);
    auto err = disk_manager.CreateFile(TEST_FILE_ID);
    ASSERT_TRUE(err.has_value());
    EXPECT_NE(dynamic_cast<yadb::CreateFileError*>(err->get()), nullptr);
}

TEST_F(DiskManagerTest, DeleteFileSucceeds)
{
    disk_manager.CreateFile(TEST_FILE_ID);
    auto err = disk_manager.DeleteFile(TEST_FILE_ID);
    EXPECT_FALSE(err.has_value());
    EXPECT_FALSE(std::filesystem::exists(TestFilePath()));
}

TEST_F(DiskManagerTest, SimpleWriteRead)
{
    disk_manager.CreateFile(TEST_FILE_ID);

    auto alloc = disk_manager.AllocatePage(TEST_FILE_ID);
    ASSERT_TRUE(alloc.has_value());
    file_page_id_t fp_id { TEST_FILE_ID, alloc.value() };

    std::array<PageData, PAGE_SIZE> write_data;
    write_data.fill(std::byte { 'A' });
    auto write_err = disk_manager.WritePage(fp_id, FullPage { write_data });
    ASSERT_FALSE(write_err.has_value());

    std::array<PageData, PAGE_SIZE> read_data;
    read_data.fill(std::byte { 0 });
    auto read_err = disk_manager.ReadPage(fp_id, MutFullPage { read_data });
    ASSERT_FALSE(read_err.has_value());

    EXPECT_EQ(read_data, write_data);
}

TEST_F(DiskManagerTest, DeletedPageIsReused)
{
    disk_manager.CreateFile(TEST_FILE_ID);

    auto alloc = disk_manager.AllocatePage(TEST_FILE_ID);
    ASSERT_TRUE(alloc.has_value());
    file_page_id_t fp_id { TEST_FILE_ID, alloc.value() };

    disk_manager.DeletePage(fp_id);

    auto realloc = disk_manager.AllocatePage(TEST_FILE_ID);
    ASSERT_TRUE(realloc.has_value());
    EXPECT_EQ(alloc.value(), realloc.value());
}

TEST_F(DiskManagerTest, FileGrowsOnAllocation)
{
    disk_manager.CreateFile(TEST_FILE_ID);

    std::array<PageData, PAGE_SIZE> write_data;
    write_data.fill(std::byte { 'B' });

    for (int i = 0; i < 8; i++) {
        auto alloc = disk_manager.AllocatePage(TEST_FILE_ID);
        ASSERT_TRUE(alloc.has_value()) << "AllocatePage failed on iteration " << i;
        file_page_id_t fp_id { TEST_FILE_ID, alloc.value() };
        auto err = disk_manager.WritePage(fp_id, FullPage { write_data });
        EXPECT_FALSE(err.has_value()) << "WritePage failed on iteration " << i;
    }
}
