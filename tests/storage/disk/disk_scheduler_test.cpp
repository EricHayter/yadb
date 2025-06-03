#include "common/type_definitions.h"
#include "storage/disk/disk_scheduler.h"
#include "storage/disk/io_tasks.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <thread>
#include <vector>
#include <future>

/**
 * \brief RAII class for creating a temporary directory for creating files in
 */
class DiskSchedulerTest : public testing::Test {
	protected:
	std::filesystem::path temp_dir{"testing_temp"};

	DiskSchedulerTest()
	{
		if (not std::filesystem::exists(temp_dir))
		{
			std::filesystem::create_directory(temp_dir);
		}
	}

	~DiskSchedulerTest()
	{
		std::filesystem::remove_all(temp_dir);
	}
};

TEST_F(DiskSchedulerTest, TestConcurrentCRUD)
{
	std::filesystem::path temp_db_file(temp_dir / "db_file");
	DiskScheduler disk_scheduler(temp_db_file);
	std::vector<std::jthread> threads;
	int thread_count = 1;

	auto thread_func = [&disk_scheduler](int i){
		// create the page
		std::promise<page_id_t> page_id_promise;
		std::future<page_id_t> page_id_future = page_id_promise.get_future();
		disk_scheduler.AllocatePage(std::move(page_id_promise));
		page_id_t page_id = page_id_future.get();

		// perform the write
		std::vector<char> write_buffer(PAGE_SIZE, i);
		PageData write_data_view(write_buffer.begin(), PAGE_SIZE);
		std::promise<void> write_promise;
		std::future<void> write_future = write_promise.get_future();
		disk_scheduler.WritePage(page_id, write_data_view, std::move(write_promise));
		write_future.get();

		// perform the read
		std::vector<char> read_buffer(PAGE_SIZE);
		PageData read_data_view(read_buffer.begin(), PAGE_SIZE);
		std::promise<void> read_promise;
		std::future<void> read_future = read_promise.get_future();
		disk_scheduler.ReadPage(page_id, read_data_view, std::move(read_promise));
		read_future.get();

		EXPECT_EQ(read_buffer, write_buffer);

		std::promise<void> delete_promise;
		std::future<void> delete_future = delete_promise.get_future();
		disk_scheduler.DeletePage(page_id, std::move(delete_promise));
		delete_future.get();
	};

	for (int i = 0; i < thread_count; i++) {
            threads.push_back(std::jthread(thread_func, i));
    }
}
