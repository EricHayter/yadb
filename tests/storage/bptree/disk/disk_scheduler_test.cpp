#include "storage/bptree/disk/disk_scheduler.h"
#include "storage/bptree/disk/io_tasks.h"
#include "common/definitions.h"
#include <filesystem>
#include <future>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

/**
 * \brief RAII class for creating a temporary directory for creating files in
 */
class DiskSchedulerTest : public testing::Test {
protected:
    std::filesystem::path temp_dir { "testing_temp" };

    DiskSchedulerTest()
    {
        if (not std::filesystem::exists(temp_dir)) {
            std::filesystem::create_directory(temp_dir);
        }
    }

    ~DiskSchedulerTest()
    {
        std::filesystem::remove_all(temp_dir);
    }
};

TEST_F(DiskSchedulerTest, TestConcurrentPageAllocation_UniquePageIds)
{
    DiskScheduler disk_scheduler {};
    const int thread_count = 2048;

    std::vector<std::jthread> threads;
    std::vector<std::future<page_id_t>> futures(thread_count);
    std::vector<std::promise<page_id_t>> promises(thread_count);

    // Launch concurrent allocation
    for (int i = 0; i < thread_count; ++i) {
        futures[i] = promises[i].get_future();
        threads.emplace_back([&disk_scheduler, p = std::move(promises[i])]() mutable {
            disk_scheduler.AllocatePage(std::move(p));
        });
    }

    // Collect page IDs
    std::set<page_id_t> page_ids;
    for (int i = 0; i < thread_count; ++i) {
        page_id_t id = futures[i].get();
        auto insert_result = page_ids.insert(id);
        EXPECT_TRUE(insert_result.second) << "Page id: " << id << " already in set\n";
    }

    // Ensure all page IDs are unique
    EXPECT_EQ(page_ids.size(), thread_count);
}

TEST_F(DiskSchedulerTest, TestConcurrentCRUD)
{
    DiskScheduler disk_scheduler {};
    std::vector<std::jthread> threads;
    int thread_count = 64;

    auto thread_func = [&disk_scheduler](int i) {
        // create the page
        std::promise<page_id_t> page_id_promise;
        std::future<page_id_t> page_id_future = page_id_promise.get_future();
        disk_scheduler.AllocatePage(std::move(page_id_promise));
        page_id_t page_id = page_id_future.get();

        // perform the write
        std::vector<PageData> write_buffer(PAGE_SIZE, PageData{static_cast<unsigned char>(i)});
        MutFullPage write_data_view(write_buffer.begin(), PAGE_SIZE);
        std::promise<bool> write_promise;
        std::future<bool> write_future = write_promise.get_future();
        disk_scheduler.WritePage(page_id, write_data_view, std::move(write_promise));
        EXPECT_TRUE(write_future.get());

        // perform the read
        std::vector<PageData> read_buffer(PAGE_SIZE, PageData{0});
        MutFullPage read_data_view(read_buffer.begin(), PAGE_SIZE);
        std::promise<bool> read_promise;
        std::future<bool> read_future = read_promise.get_future();
        disk_scheduler.ReadPage(page_id, read_data_view, std::move(read_promise));
        EXPECT_TRUE(read_future.get());

        EXPECT_EQ(read_buffer, write_buffer) << "Integrity issue on " << page_id << "\n";

        std::promise<void> delete_promise;
        std::future<void> delete_future = delete_promise.get_future();
        disk_scheduler.DeletePage(page_id, std::move(delete_promise));
        delete_future.get();
    };

    for (int i = 0; i < thread_count; i++) {
        threads.push_back(std::jthread(thread_func, i));
    }
}
