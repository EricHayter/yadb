#pragma once
#include "config/config.h"
#include "storage/disk/disk_manager.h"
#include "storage/disk/io_tasks.h"
#include <condition_variable>
#include <future>
#include <queue>
#include <stop_token>
#include <thread>

/**
 * @brief Disk Scheduler
 *
 * The disk scheduler allows for asynchronous operations to the disk manager
 * to be performed. The scheduler consists of a task queue and a worker thread
 * to perform each of the types of operations. The public interface includes
 * functions to safely create task objects and enqueue them in a thread-safe
 * manner.
 */
class DiskScheduler {
public:
    DiskScheduler();
    DiskScheduler(const DatabaseConfig& config);
    ~DiskScheduler();

    /**
     * @brief Request the creation of a new page
     *
     * @param result a promise containing the newly created page
     */
    void AllocatePage(std::promise<page_id_t>&& result);

    /**
     * @brief Request to delete page
     *
     * @param done a promise indicating when the action is complete
     */
    void DeletePage(page_id_t page_id, std::promise<void>&& done);

    /**
     * @brief Request to read a page
     *
     * @param page_id the page to read from
     * @param data the span to write the data into
     * @param status a promise containing the success status of the read
     */
    void ReadPage(page_id_t page_id, MutPageView data, std::promise<bool>&& status);

    /**
     * @brief Request to write to a page
     *
     * @param page_id the page to write to
     * @param data the span containing the data to write to the page
     * @param status a promise containing the success status of the write
     */
    void WritePage(page_id_t page_id, PageView data, std::promise<bool>&& status);

private:
    /**
     * @brief Worker thread function to handle requests
     *
     * @param stop_token stop token for stopping worker thread
     */
    void WorkerFunction(std::stop_token stop_token);

    /// thread-safe task queue with a condition variable to notify worker thread
    /// of task arrival
    std::condition_variable cv_m;
    std::mutex mut_m;
    std::queue<IOTasks::Task> tasks_m;

    /// underlying disk manager for worker thread to perform I/O operations
    DiskManager disk_manager_m;
    std::shared_ptr<spdlog::logger> logger_m;
    std::jthread worker_thread_m;
};
