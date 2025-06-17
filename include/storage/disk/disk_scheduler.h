#pragma once
#include "common/type_definitions.h"
#include "storage/disk/disk_manager.h"
#include "storage/disk/io_tasks.h"
#include <condition_variable>
#include <filesystem>
#include <future>
#include <optional>
#include <queue>
#include <stop_token>
#include <thread>

/* Disk Scheduler
 * The disk scheduler allows for asynchronous operations to the disk manager
 * to be performed. The scheduler consists of a task queue and a worker thread
 * to perform each of the types of operations. The public interface includes
 * functions to safely create task objects and enqueue them in a thread-safe
 * manner.
 */
class DiskScheduler {
public:
    DiskScheduler(const std::filesystem::path& db_file);
    ~DiskScheduler();

    void AllocatePage(std::promise<page_id_t>&& result);
    void DeletePage(page_id_t page_id, std::promise<void>&& done);
    void ReadPage(page_id_t page_id, MutPageView data, std::promise<bool>&& status);
    void WritePage(page_id_t page_id, PageView data, std::promise<bool>&& status);

private:
    void WorkerFunction(std::stop_token stop_token);
    std::jthread worker_thread_m;

    // thread-safe task queue with a condition variable to notify worker thread
    // of task arrival
    std::condition_variable cv_m;
    std::mutex mut_m;
    std::queue<IOTasks::Task> tasks_m;

    // underlying disk manager for worker thread to perform I/O operations
    DiskManager disk_manager_m;
};
