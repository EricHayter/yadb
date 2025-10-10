#include "core/shared_spinlock.h"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

class SharedSpinlockTest : public ::testing::Test {
protected:
    SharedSpinlock lock;
};

TEST_F(SharedSpinlockTest, BasicExclusiveLock)
{
    lock.lock();
    lock.unlock();

    ASSERT_TRUE(lock.try_lock());
    lock.unlock();
}

TEST_F(SharedSpinlockTest, BasicSharedLock)
{
    lock.lock_shared();
    lock.unlock_shared();

    ASSERT_TRUE(lock.try_lock_shared());
    lock.unlock_shared();
}

TEST_F(SharedSpinlockTest, MultipleReadersConcurrent)
{
    std::atomic<int> concurrent_readers { 0 };
    std::atomic<int> max_concurrent { 0 };

    const int NUM_READERS = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_READERS; i++) {
        threads.emplace_back([&]() {
            lock.lock_shared();

            int current = concurrent_readers.fetch_add(1) + 1;
            int prev_max = max_concurrent.load();
            while (prev_max < current && !max_concurrent.compare_exchange_weak(prev_max, current))
                ;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            concurrent_readers.fetch_sub(1);
            lock.unlock_shared();
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(max_concurrent.load(), 1) << "Multiple readers should hold lock simultaneously";
    EXPECT_EQ(concurrent_readers.load(), 0) << "All readers should have released";
}

TEST_F(SharedSpinlockTest, WriterExcludesReaders)
{
    std::atomic<bool> writer_has_lock { false };
    std::atomic<bool> reader_violated { false };

    std::thread writer([&]() {
        lock.lock();
        writer_has_lock.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        writer_has_lock.store(false);
        lock.unlock();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::thread reader([&]() {
        lock.lock_shared();
        if (writer_has_lock.load()) {
            reader_violated.store(true);
        }
        lock.unlock_shared();
    });

    writer.join();
    reader.join();

    EXPECT_FALSE(reader_violated.load()) << "Reader acquired lock while writer held it";
}

TEST_F(SharedSpinlockTest, ReaderExcludesWriter)
{
    std::atomic<bool> reader_has_lock { false };
    std::atomic<bool> writer_violated { false };

    std::thread reader([&]() {
        lock.lock_shared();
        reader_has_lock.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        reader_has_lock.store(false);
        lock.unlock_shared();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::thread writer([&]() {
        lock.lock();
        if (reader_has_lock.load()) {
            writer_violated.store(true);
        }
        lock.unlock();
    });

    reader.join();
    writer.join();

    EXPECT_FALSE(writer_violated.load()) << "Writer acquired lock while reader held it";
}

TEST_F(SharedSpinlockTest, TryLockFailsWhenLocked)
{
    lock.lock();
    EXPECT_FALSE(lock.try_lock()) << "try_lock should fail when exclusively locked";
    lock.unlock();

    lock.lock_shared();
    EXPECT_FALSE(lock.try_lock()) << "try_lock should fail when shared locked";
    lock.unlock_shared();
}

TEST_F(SharedSpinlockTest, TryLockSharedFailsWhenExclusivelyLocked)
{
    lock.lock();
    EXPECT_FALSE(lock.try_lock_shared()) << "try_lock_shared should fail when exclusively locked";
    lock.unlock();
}

TEST_F(SharedSpinlockTest, TryLockSharedSucceedsWhenSharedLocked)
{
    lock.lock_shared();
    EXPECT_TRUE(lock.try_lock_shared()) << "try_lock_shared should succeed when already shared locked";
    lock.unlock_shared();
    lock.unlock_shared();
}

TEST_F(SharedSpinlockTest, StressMixedReadersWriters)
{
    std::atomic<int> shared_counter { 0 };
    std::atomic<bool> violation_detected { false };
    const int ITERATIONS = 1000;

    auto writer_func = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            lock.lock();
            int val = shared_counter.load();
            shared_counter.store(val + 1);

            // Verify exclusive access
            if (shared_counter.load() != val + 1) {
                violation_detected.store(true);
            }
            lock.unlock();
        }
    };

    auto reader_func = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            lock.lock_shared();
            int val1 = shared_counter.load();
            int val2 = shared_counter.load();

            // Counter shouldn't change during read
            if (val1 != val2) {
                violation_detected.store(true);
            }
            lock.unlock_shared();
        }
    };

    std::vector<std::thread> threads;
    threads.emplace_back(writer_func);
    threads.emplace_back(writer_func);
    threads.emplace_back(reader_func);
    threads.emplace_back(reader_func);
    threads.emplace_back(reader_func);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(violation_detected.load()) << "Synchronization violation detected";
    EXPECT_EQ(shared_counter.load(), ITERATIONS * 2) << "Counter should equal total writes";
}

TEST_F(SharedSpinlockTest, AlternatingAccess)
{
    std::atomic<int> sequence { 0 };
    std::atomic<bool> ordering_violation { false };
    const int ROUNDS = 100;

    auto writer = [&]() {
        for (int i = 0; i < ROUNDS; i++) {
            lock.lock();
            int expected = i * 2;
            if (sequence.load() != expected) {
                ordering_violation.store(true);
            }
            sequence.fetch_add(1);
            lock.unlock();
        }
    };

    auto reader = [&]() {
        for (int i = 0; i < ROUNDS; i++) {
            lock.lock_shared();
            int expected = i * 2 + 1;
            while (sequence.load() < expected) {
                lock.unlock_shared();
                std::this_thread::yield();
                lock.lock_shared();
            }
            sequence.fetch_add(1);
            lock.unlock_shared();
        }
    };

    std::thread t1(writer);
    std::thread t2(reader);

    t1.join();
    t2.join();

    EXPECT_FALSE(ordering_violation.load()) << "Ordering violation detected";
}

TEST_F(SharedSpinlockTest, RealisticBufferPoolWorkload)
{
    std::atomic<int> data { 0 };
    std::atomic<int> read_count { 0 };
    std::atomic<int> write_count { 0 };
    std::atomic<bool> violation { false };

    const int NUM_READERS = 8;
    const int NUM_WRITERS = 2;
    const int OPS_PER_THREAD = 500;

    auto reader = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            lock.lock_shared();
            int val = data.load();
            // Simulate reading a page
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            if (data.load() != val) {
                violation.store(true);
            }
            read_count.fetch_add(1);
            lock.unlock_shared();
        }
    };

    auto writer = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            lock.lock();
            int old_val = data.load();
            data.store(old_val + 1);
            // Simulate writing to a page
            std::this_thread::sleep_for(std::chrono::microseconds(5));
            if (data.load() != old_val + 1) {
                violation.store(true);
            }
            write_count.fetch_add(1);
            lock.unlock();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_READERS; i++) {
        threads.emplace_back(reader);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        threads.emplace_back(writer);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(violation.load()) << "Data race detected";
    EXPECT_EQ(read_count.load(), NUM_READERS * OPS_PER_THREAD);
    EXPECT_EQ(write_count.load(), NUM_WRITERS * OPS_PER_THREAD);
    EXPECT_EQ(data.load(), NUM_WRITERS * OPS_PER_THREAD);
}

TEST_F(SharedSpinlockTest, SequentialLockUnlockCycles)
{
    const int CYCLES = 1000;

    for (int i = 0; i < CYCLES; i++) {
        lock.lock();
        lock.unlock();
    }

    for (int i = 0; i < CYCLES; i++) {
        lock.lock_shared();
        lock.unlock_shared();
    }

    SUCCEED() << "Completed " << CYCLES << " lock/unlock cycles";
}

TEST_F(SharedSpinlockTest, WriterEventuallyProceeds)
{
    std::atomic<bool> writer_completed { false };
    std::atomic<bool> stop_readers { false };

    // Start continuous readers
    std::vector<std::thread> readers;
    for (int i = 0; i < 5; i++) {
        readers.emplace_back([&]() {
            while (!stop_readers.load()) {
                lock.lock_shared();
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                lock.unlock_shared();
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Writer should eventually get the lock
    std::thread writer([&]() {
        lock.lock();
        writer_completed.store(true);
        lock.unlock();
    });

    // Wait a reasonable time for writer
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    stop_readers.store(true);

    for (auto& r : readers) {
        r.join();
    }
    writer.join();

    EXPECT_TRUE(writer_completed.load()) << "Writer should eventually acquire lock";
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
