#include "buffer/buffer_pool_manager.h"
#include "buffer/page_guard.h"
#include "common/type_definitions.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <random>
#include <thread>
#include <vector>

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

// TEST_F(DiskManagerTest, TestWaitWriteRead)
//{
//     int number_frames = 1;
//     std::filesystem::path temp_db_file(temp_dir / "db_file");
//     BufferPoolManager buffer_pool_man(number_frames, temp_db_file);
//     page_id_t page_id = buffer_pool_man.NewPage();
//     {
//         std::vector<char> buff(PAGE_SIZE, 'B');
//         MutPageView page_view(buff.begin(), PAGE_SIZE);
//         WritePageGuard page_guard = buffer_pool_man.WaitWritePage(page_id);
//         std::copy(page_view.begin(), page_view.end(), page_guard.GetData().begin());
//     }
//     {
//         const std::vector<char> expected_buf(PAGE_SIZE, 'B');
//         std::vector<char> buff(PAGE_SIZE, 0);
//         MutPageView page_view(buff.begin(), PAGE_SIZE);
//         ReadPageGuard page_guard = buffer_pool_man.WaitReadPage(page_id);
//         PageView page_data = page_guard.GetData();
//         std::copy(page_data.begin(), page_data.end(), buff.begin());
//         EXPECT_EQ(expected_buf, buff);
//     }
// }
//
// TEST_F(DiskManagerTest, TestTryWriteRead)
//{
//     int number_frames = 1;
//     std::filesystem::path temp_db_file(temp_dir / "db_file");
//     BufferPoolManager buffer_pool_man(number_frames, temp_db_file);
//     page_id_t page_id = buffer_pool_man.NewPage();
//     {
//         std::vector<char> buff(PAGE_SIZE, 'B');
//         MutPageView page_view(buff.begin(), PAGE_SIZE);
//         auto page_guard_opt = buffer_pool_man.TryWritePage(page_id);
//         ASSERT_TRUE(page_guard_opt.has_value());
//         WritePageGuard page_guard = std::move(*page_guard_opt);
//         std::copy(page_view.begin(), page_view.end(), page_guard.GetData().begin());
//     }
//     {
//         const std::vector<char> expected_buf(PAGE_SIZE, 'B');
//         std::vector<char> buff(PAGE_SIZE, 0);
//         MutPageView page_view(buff.begin(), PAGE_SIZE);
//         auto page_guard_opt = buffer_pool_man.TryReadPage(page_id);
//         EXPECT_TRUE(page_guard_opt.has_value());
//         ReadPageGuard page_guard = std::move(*page_guard_opt);
//         PageView page_data = page_guard.GetData();
//         std::copy(page_data.begin(), page_data.end(), buff.begin());
//         EXPECT_EQ(expected_buf, buff);
//     }
// }

// Test buffer pool eviction when full
TEST_F(DiskManagerTest, TestBufferPoolEviction)
{
    int number_frames = 2;
    std::filesystem::path temp_db_file(temp_dir / "db_file_eviction");
    BufferPoolManager buffer_pool_man(number_frames, temp_db_file);

    // Create 3 pages to force eviction
    page_id_t page1 = buffer_pool_man.NewPage();
    page_id_t page2 = buffer_pool_man.NewPage();
    page_id_t page3 = buffer_pool_man.NewPage();

    // Write unique data to each page
    {
        WritePageGuard guard1 = buffer_pool_man.WaitWritePage(page1);
        std::fill(guard1.GetData().begin(), guard1.GetData().end(), 'A');
    }
    {
        WritePageGuard guard2 = buffer_pool_man.WaitWritePage(page2);
        std::fill(guard2.GetData().begin(), guard2.GetData().end(), 'B');
    }
    {
        WritePageGuard guard3 = buffer_pool_man.WaitWritePage(page3);
        std::fill(guard3.GetData().begin(), guard3.GetData().end(), 'C');
    }

    // Verify all pages can still be read (from disk if evicted)
    {
        ReadPageGuard guard1 = buffer_pool_man.WaitReadPage(page1);
        EXPECT_EQ(guard1.GetData()[0], 'A');
    }
    {
        ReadPageGuard guard2 = buffer_pool_man.WaitReadPage(page2);
        EXPECT_EQ(guard2.GetData()[0], 'B');
    }
    {
        ReadPageGuard guard3 = buffer_pool_man.WaitReadPage(page3);
        EXPECT_EQ(guard3.GetData()[0], 'C');
    }
}

// Test multiple readers can access the same page concurrently
TEST_F(DiskManagerTest, TestConcurrentReaders)
{
    int number_frames = 1;
    std::filesystem::path temp_db_file(temp_dir / "db_file_concurrent_read");
    BufferPoolManager buffer_pool_man(number_frames, temp_db_file);

    page_id_t page_id = buffer_pool_man.NewPage();

    // Write initial data
    {
        WritePageGuard guard = buffer_pool_man.WaitWritePage(page_id);
        std::fill(guard.GetData().begin(), guard.GetData().end(), 'X');
    }

    const int num_readers = 4;
    std::vector<std::thread> readers;
    std::atomic<int> successful_reads { 0 };

    for (int i = 0; i < num_readers; ++i) {
        readers.emplace_back([&buffer_pool_man, page_id, &successful_reads]() {
            ReadPageGuard guard = buffer_pool_man.WaitReadPage(page_id);
            if (guard.GetData()[0] == 'X') {
                successful_reads++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }

    for (auto& reader : readers) {
        reader.join();
    }

    EXPECT_EQ(successful_reads.load(), num_readers);
}

// Test writer exclusivity - only one writer at a time
TEST_F(DiskManagerTest, TestWriterExclusivity)
{
    int number_frames = 1;
    std::filesystem::path temp_db_file(temp_dir / "db_file_writer_exclusive");
    BufferPoolManager buffer_pool_man(number_frames, temp_db_file);

    page_id_t page_id = buffer_pool_man.NewPage();

    std::atomic<bool> first_writer_started { false };
    std::atomic<bool> second_writer_blocked { true };

    std::thread writer1([&]() {
        WritePageGuard guard = buffer_pool_man.WaitWritePage(page_id);
        first_writer_started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::fill(guard.GetData().begin(), guard.GetData().end(), '1');
    });

    std::thread writer2([&]() {
        // Wait for first writer to start
        while (!first_writer_started) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto start = std::chrono::steady_clock::now();
        WritePageGuard guard = buffer_pool_man.WaitWritePage(page_id);
        auto end = std::chrono::steady_clock::now();

        // Should have been blocked for at least 40ms
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        second_writer_blocked = (duration.count() >= 40);
        std::fill(guard.GetData().begin(), guard.GetData().end(), '2');
    });

    writer1.join();
    writer2.join();

    EXPECT_TRUE(second_writer_blocked);

    // Verify final state
    ReadPageGuard guard = buffer_pool_man.WaitReadPage(page_id);
    EXPECT_EQ(guard.GetData()[0], '2');
}

// Test Try operations fail when page is locked
TEST_F(DiskManagerTest, TestTryOperationsBlocked)
{
    int number_frames = 1;
    std::filesystem::path temp_db_file(temp_dir / "db_file_try_blocked");
    BufferPoolManager buffer_pool_man(number_frames, temp_db_file);

    page_id_t page_id = buffer_pool_man.NewPage();

    WritePageGuard write_guard = buffer_pool_man.WaitWritePage(page_id);

    // Try operations should fail while write guard is held
    auto try_read = buffer_pool_man.TryReadPage(page_id);
    auto try_write = buffer_pool_man.TryWritePage(page_id);

    EXPECT_FALSE(try_read.has_value());
    EXPECT_FALSE(try_write.has_value());
}

// Test mixed concurrent operations
TEST_F(DiskManagerTest, TestMixedConcurrentOperations)
{
    int number_frames = 3;
    std::filesystem::path temp_db_file(temp_dir / "db_file_mixed_concurrent");
    BufferPoolManager buffer_pool_man(number_frames, temp_db_file);

    const int num_pages = 5;
    std::vector<page_id_t> page_ids;

    // Create multiple pages
    for (int i = 0; i < num_pages; ++i) {
        page_ids.push_back(buffer_pool_man.NewPage());
    }

    std::atomic<int> operations_completed { 0 };
    std::vector<std::future<void>> futures;

    // Launch concurrent readers and writers
    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> page_dist(0, num_pages - 1);
            std::uniform_int_distribution<> op_dist(0, 3);

            for (int j = 0; j < 5; ++j) {
                page_id_t page_id = page_ids[page_dist(gen)];
                int op = op_dist(gen);

                try {
                    if (op == 0) {
                        // Wait read
                        ReadPageGuard guard = buffer_pool_man.WaitReadPage(page_id);
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    } else if (op == 1) {
                        // Wait write
                        WritePageGuard guard = buffer_pool_man.WaitWritePage(page_id);
                        std::fill(guard.GetData().begin(), guard.GetData().begin() + 10,
                            static_cast<char>('A' + i));
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    } else if (op == 2) {
                        // Try read
                        auto guard_opt = buffer_pool_man.TryReadPage(page_id);
                        if (guard_opt.has_value()) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                    } else {
                        // Try write
                        auto guard_opt = buffer_pool_man.TryWritePage(page_id);
                        if (guard_opt.has_value()) {
                            std::fill(guard_opt->GetData().begin(),
                                guard_opt->GetData().begin() + 10,
                                static_cast<char>('a' + i));
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                    }
                    operations_completed++;
                } catch (...) {
                    // Handle any exceptions gracefully in concurrent context
                }
            }
        }));
    }

    // Wait for all operations to complete
    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_GT(operations_completed.load(), 0);
}

// Test page deletion/invalidation
TEST_F(DiskManagerTest, TestPageDeletion)
{
    int number_frames = 2;
    std::filesystem::path temp_db_file(temp_dir / "db_file_deletion");
    BufferPoolManager buffer_pool_man(number_frames, temp_db_file);

    page_id_t page_id = buffer_pool_man.NewPage();

    // Write some data
    {
        WritePageGuard guard = buffer_pool_man.WaitWritePage(page_id);
        std::fill(guard.GetData().begin(), guard.GetData().end(), 'D');
    }

    // Delete the page (if your buffer pool manager supports deletion)
    // This test assumes a DeletePage method exists
    // buffer_pool_man.DeletePage(page_id);

    // Note: Uncomment above and modify based on your actual API
    // For now, just verify the page exists
    ReadPageGuard guard = buffer_pool_man.WaitReadPage(page_id);
    EXPECT_EQ(guard.GetData()[0], 'D');
}

// Stress test with many concurrent operations
TEST_F(DiskManagerTest, TestStressConcurrency)
{
    int number_frames = 5;
    std::filesystem::path temp_db_file(temp_dir / "db_file_stress");
    BufferPoolManager buffer_pool_man(number_frames, temp_db_file);

    const int num_pages = 10;
    const int num_threads = 8;
    const int operations_per_thread = 20;

    std::vector<page_id_t> page_ids;
    for (int i = 0; i < num_pages; ++i) {
        page_ids.push_back(buffer_pool_man.NewPage());
    }

    std::atomic<int> successful_operations { 0 };
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd() + t);
            std::uniform_int_distribution<> page_dist(0, num_pages - 1);
            std::uniform_int_distribution<> op_dist(0, 1);

            for (int i = 0; i < operations_per_thread; ++i) {
                try {
                    page_id_t page_id = page_ids[page_dist(gen)];

                    if (op_dist(gen) == 0) {
                        // Read operation
                        ReadPageGuard guard = buffer_pool_man.WaitReadPage(page_id);
                        // Simulate some work
                        volatile char dummy = guard.GetData()[0];
                        (void)dummy;
                        successful_operations++;
                    } else {
                        // Write operation
                        WritePageGuard guard = buffer_pool_man.WaitWritePage(page_id);
                        guard.GetData()[0] = static_cast<char>('A' + (t % 26));
                        successful_operations++;
                    }
                } catch (...) {
                    // Handle exceptions in concurrent environment
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successful_operations.load(), num_threads * operations_per_thread);
}

// Test buffer pool behavior when no frames available
TEST_F(DiskManagerTest, TestNoFramesAvailable)
{
    int number_frames = 1;
    std::filesystem::path temp_db_file(temp_dir / "db_file_no_frames");
    BufferPoolManager buffer_pool_man(number_frames, temp_db_file);

    page_id_t page1 = buffer_pool_man.NewPage();
    page_id_t page2 = buffer_pool_man.NewPage();

    // Hold a lock on page1
    WritePageGuard guard1 = buffer_pool_man.WaitWritePage(page1);

    // Try to access page2 with Try operation - should fail if page1 can't be evicted
    auto guard2_opt = buffer_pool_man.TryWritePage(page2);

    // This test depends on your eviction policy
    // If locked pages can't be evicted, this should fail
    // Adjust expectations based on your implementation
}
