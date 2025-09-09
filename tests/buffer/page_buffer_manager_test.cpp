#include "buffer/page_buffer_manager.h"
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
    DiskManagerTest() = default;
    ~DiskManagerTest()
    {
        std::filesystem::remove_all(temp_dir);
    }
};

TEST_F(DiskManagerTest, TestWaitWriteRead)
{
    std::vector<char> data_buffer(26);
    for (char i = 0; i < data_buffer.size(); i++) {
        data_buffer[i] = 'a' + i;
    }

    int number_frames = 1;
    PageBufferManager page_buffer_man(temp_dir, number_frames);
    page_id_t page_id = page_buffer_man.NewPage();
    slot_id_t slot_id;
    {
        std::optional<PageMut> page = page_buffer_man.WaitWritePage(page_id);
        EXPECT_TRUE(page.has_value());
        auto slot_id = page->AllocateSlot(data_buffer.size());
        EXPECT_TRUE(slot_id.has_value());
        page->WriteSlot(*slot_id, data_buffer);
    }
    {
        std::optional<Page> page = page_buffer_man.WaitReadPage(page_id);
        EXPECT_TRUE(page.has_value());
        PageView page_data = page->ReadPage();
        std::vector<char> read_buffer(page_data.begin(), page_data.begin() + data_buffer.size());
        EXPECT_EQ(data_buffer, read_buffer);
    }
}

//TEST_F(DiskManagerTest, TestTryWriteRead)
//{
//    int number_frames = 1;
//    PageBufferManager page_buffer_man(temp_dir, number_frames);
//    page_id_t page_id = page_buffer_man.NewPage();
//    {
//        std::vector<char> buff(PAGE_SIZE, 'B');
//        MutPageView page_view(buff.begin(), PAGE_SIZE);
//        auto page_guard_opt = page_buffer_man.TryWritePage(page_id);
//        ASSERT_TRUE(page_guard_opt.has_value());
//        WritePageGuard page_guard = std::move(*page_guard_opt);
//        std::copy(page_view.begin(), page_view.end(), page_guard.GetData().begin());
//    }
//    {
//        const std::vector<char> expected_buf(PAGE_SIZE, 'B');
//        std::vector<char> buff(PAGE_SIZE, 0);
//        MutPageView page_view(buff.begin(), PAGE_SIZE);
//        auto page_guard_opt = page_buffer_man.TryReadPage(page_id);
//        EXPECT_TRUE(page_guard_opt.has_value());
//        ReadPageGuard page_guard = std::move(*page_guard_opt);
//        PageView page_data = page_guard.GetData();
//        std::copy(page_data.begin(), page_data.end(), buff.begin());
//        EXPECT_EQ(expected_buf, buff);
//    }
//}
//
//// Test page buffer eviction when full
//TEST_F(DiskManagerTest, TestPageBufferEviction)
//{
//    int number_frames = 2;
//    PageBufferManager page_buffer_man(temp_dir, number_frames);
//
//    // Create 3 pages to force eviction
//    page_id_t page1 = page_buffer_man.NewPage();
//    page_id_t page2 = page_buffer_man.NewPage();
//    page_id_t page3 = page_buffer_man.NewPage();
//
//    // Write unique data to each page
//    {
//        std::optional<WritePageGuard> guard1 = page_buffer_man.WaitWritePage(page1);
//        ASSERT_TRUE(guard1.has_value());
//        std::fill(guard1->GetData().begin(), guard1->GetData().end(), 'A');
//    }
//    {
//        std::optional<WritePageGuard> guard2 = page_buffer_man.WaitWritePage(page2);
//        std::fill(guard2->GetData().begin(), guard2->GetData().end(), 'B');
//    }
//    {
//        std::optional<WritePageGuard> guard3 = page_buffer_man.WaitWritePage(page3);
//        std::fill(guard3->GetData().begin(), guard3->GetData().end(), 'C');
//    }
//
//    // Verify all pages can still be read (from disk if evicted)
//    {
//        std::optional<ReadPageGuard> guard1 = page_buffer_man.WaitReadPage(page1);
//        EXPECT_EQ(guard1->GetData()[0], 'A');
//    }
//    {
//        std::optional<ReadPageGuard> guard2 = page_buffer_man.WaitReadPage(page2);
//        EXPECT_EQ(guard2->GetData()[0], 'B');
//    }
//    {
//        std::optional<ReadPageGuard> guard3 = page_buffer_man.WaitReadPage(page3);
//        EXPECT_EQ(guard3->GetData()[0], 'C');
//    }
//}
//
//// Test multiple readers can access the same page concurrently
//TEST_F(DiskManagerTest, TestConcurrentReaders)
//{
//    int number_frames = 1;
//    PageBufferManager page_buffer_man(temp_dir, number_frames);
//
//    page_id_t page_id = page_buffer_man.NewPage();
//
//    // Write initial data
//    {
//        std::optional<WritePageGuard> guard = page_buffer_man.WaitWritePage(page_id);
//        ASSERT_TRUE(guard.has_value());
//        std::fill(guard->GetData().begin(), guard->GetData().end(), 'X');
//    }
//
//    const int num_readers = 4;
//    std::vector<std::thread> readers;
//    std::atomic<int> successful_reads { 0 };
//
//    for (int i = 0; i < num_readers; ++i) {
//        readers.emplace_back([&page_buffer_man, page_id, &successful_reads]() {
//            std::optional<ReadPageGuard> guard = page_buffer_man.WaitReadPage(page_id);
//            ASSERT_TRUE(guard.has_value());
//            if (guard->GetData()[0] == 'X') {
//                successful_reads++;
//            }
//            std::this_thread::sleep_for(std::chrono::milliseconds(10));
//        });
//    }
//
//    for (auto& reader : readers) {
//        reader.join();
//    }
//
//    EXPECT_EQ(successful_reads.load(), num_readers);
//}
//
//// Test writer exclusivity - only one writer at a time
//TEST_F(DiskManagerTest, TestWriterExclusivity)
//{
//    int number_frames = 1;
//    PageBufferManager page_buffer_man(temp_dir, number_frames);
//
//    page_id_t page_id = page_buffer_man.NewPage();
//
//    std::atomic<bool> first_writer_started { false };
//    std::atomic<bool> second_writer_blocked { true };
//
//    std::thread writer1([&]() {
//        std::optional<WritePageGuard> guard = page_buffer_man.WaitWritePage(page_id);
//        ASSERT_TRUE(guard.has_value());
//        first_writer_started = true;
//        std::this_thread::sleep_for(std::chrono::milliseconds(50));
//        std::fill(guard->GetData().begin(), guard->GetData().end(), '1');
//    });
//
//    std::thread writer2([&]() {
//        // Wait for first writer to start
//        while (!first_writer_started) {
//            std::this_thread::sleep_for(std::chrono::milliseconds(1));
//        }
//
//        auto start = std::chrono::steady_clock::now();
//        std::optional<WritePageGuard> guard = page_buffer_man.WaitWritePage(page_id);
//        ASSERT_TRUE(guard.has_value());
//        auto end = std::chrono::steady_clock::now();
//
//        // Should have been blocked for at least 40ms
//        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//        second_writer_blocked = (duration.count() >= 40);
//        std::fill(guard->GetData().begin(), guard->GetData().end(), '2');
//    });
//
//    writer1.join();
//    writer2.join();
//
//    EXPECT_TRUE(second_writer_blocked);
//
//    // Verify final state
//    std::optional<ReadPageGuard> guard = page_buffer_man.WaitReadPage(page_id);
//    ASSERT_TRUE(guard.has_value());
//    EXPECT_EQ(guard->GetData()[0], '2');
//}
//
//// Test Try operations fail when page is locked
//TEST_F(DiskManagerTest, TestTryOperationsBlocked)
//{
//    int number_frames = 1;
//    PageBufferManager page_buffer_man(temp_dir, number_frames);
//
//    page_id_t page_id = page_buffer_man.NewPage();
//
//    std::optional<WritePageGuard> write_guard = page_buffer_man.WaitWritePage(page_id);
//    EXPECT_TRUE(write_guard.has_value());
//
//    // Try operations should fail while write guard is held
//    auto try_read = page_buffer_man.TryReadPage(page_id);
//    auto try_write = page_buffer_man.TryWritePage(page_id);
//
//    EXPECT_FALSE(try_read.has_value());
//    EXPECT_FALSE(try_write.has_value());
//}
//
//// Test mixed concurrent operations
//TEST_F(DiskManagerTest, TestMixedConcurrentOperations)
//{
//    int number_frames = 3;
//    PageBufferManager page_buffer_man(temp_dir, number_frames);
//
//    const int num_pages = 5;
//    std::vector<page_id_t> page_ids;
//
//    // Create multiple pages
//    for (int i = 0; i < num_pages; ++i) {
//        page_ids.push_back(page_buffer_man.NewPage());
//    }
//
//    std::atomic<int> operations_completed { 0 };
//    std::vector<std::future<void>> futures;
//
//    // Launch concurrent readers and writers
//    for (int i = 0; i < 10; ++i) {
//        futures.push_back(std::async(std::launch::async, [&, i]() {
//            std::random_device rd;
//            std::mt19937 gen(rd());
//            std::uniform_int_distribution<> page_dist(0, num_pages - 1);
//            std::uniform_int_distribution<> op_dist(0, 3);
//
//            for (int j = 0; j < 5; ++j) {
//                page_id_t page_id = page_ids[page_dist(gen)];
//                int op = op_dist(gen);
//
//                if (op == 0) {
//                    // Wait read
//                    std::optional<ReadPageGuard> guard = page_buffer_man.WaitReadPage(page_id);
//                    ASSERT_TRUE(guard.has_value());
//                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
//                } else if (op == 1) {
//                    // Wait write
//                    std::optional<WritePageGuard> guard = page_buffer_man.WaitWritePage(page_id);
//                    ASSERT_TRUE(guard.has_value());
//                    std::fill(guard->GetData().begin(), guard->GetData().begin() + 10,
//                        static_cast<char>('A' + i));
//                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
//                } else if (op == 2) {
//                    // Try read
//                    auto guard_opt = page_buffer_man.TryReadPage(page_id);
//                    if (guard_opt.has_value()) {
//                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
//                    }
//                } else {
//                    // Try write
//                    auto guard_opt = page_buffer_man.TryWritePage(page_id);
//                    if (guard_opt.has_value()) {
//                        std::fill(guard_opt->GetData().begin(),
//                            guard_opt->GetData().begin() + 10,
//                            static_cast<char>('a' + i));
//                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
//                    }
//                }
//                operations_completed++;
//            }
//        }));
//    }
//
//    // Wait for all operations to complete
//    for (auto& future : futures) {
//        future.wait();
//    }
//
//    EXPECT_GT(operations_completed.load(), 0);
//}
//
//// Stress test with many concurrent operations
//TEST_F(DiskManagerTest, TestStressConcurrency)
//{
//    int number_frames = 5;
//    PageBufferManager page_buffer_man(temp_dir, number_frames);
//
//    const int num_pages = 10;
//    const int num_threads = 8;
//    const int operations_per_thread = 20;
//
//    std::vector<page_id_t> page_ids;
//    for (int i = 0; i < num_pages; ++i) {
//        page_ids.push_back(page_buffer_man.NewPage());
//    }
//
//    std::atomic<int> successful_operations { 0 };
//    std::vector<std::thread> threads;
//
//    for (int t = 0; t < num_threads; ++t) {
//        threads.emplace_back([&, t]() {
//            std::random_device rd;
//            std::mt19937 gen(rd() + t);
//            std::uniform_int_distribution<> page_dist(0, num_pages - 1);
//            std::uniform_int_distribution<> op_dist(0, 1);
//
//            for (int i = 0; i < operations_per_thread; ++i) {
//                try {
//                    page_id_t page_id = page_ids[page_dist(gen)];
//
//                    if (op_dist(gen) == 0) {
//                        // Read operation
//                        std::optional<ReadPageGuard> guard = page_buffer_man.WaitReadPage(page_id);
//                        ASSERT_TRUE(guard.has_value());
//                        // Simulate some work
//                        volatile char dummy = guard->GetData()[0];
//                        (void)dummy;
//                        successful_operations++;
//                    } else {
//                        // Write operation
//                        std::optional<WritePageGuard> guard = page_buffer_man.WaitWritePage(page_id);
//                        ASSERT_TRUE(guard.has_value());
//                        guard->GetData()[0] = static_cast<char>('A' + (t % 26));
//                        successful_operations++;
//                    }
//                } catch (...) {
//                }
//            }
//        });
//    }
//
//    for (auto& thread : threads) {
//        thread.join();
//    }
//
//    EXPECT_EQ(successful_operations.load(), num_threads * operations_per_thread);
//}
//
//// Test page buffer manager behavior when no frames available
//TEST_F(DiskManagerTest, TestNoFramesAvailable)
//{
//    int number_frames = 1;
//    PageBufferManager page_buffer_man(temp_dir, number_frames);
//
//    page_id_t page1 = page_buffer_man.NewPage();
//    page_id_t page2 = page_buffer_man.NewPage();
//
//    // Hold a lock on page1
//    std::optional<WritePageGuard> guard1 = page_buffer_man.WaitWritePage(page1);
//    ASSERT_TRUE(guard1.has_value());
//
//    // Try to access page2 with Try operation - should fail if page1 can't be evicted
//    auto guard2_opt = page_buffer_man.TryWritePage(page2);
//}
