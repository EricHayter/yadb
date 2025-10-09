#pragma once

#include <condition_variable>
#include <future>
#include <queue>
#include <stop_token>
#include <thread>

#include "config/config.h"
#include "storage/bptree/disk/disk_manager.h"
#include "storage/bptree/disk/io_tasks.h"

/*
 * Disk Scheduler
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

    /*
     * Request the creation of a new page
     *
     * Requires a promise containing the newly created page id for a callback
     */
    void AllocatePage(std::promise<page_id_t>&& result);

    /*
     * Request to delete page
     *
     * Requires a void promise to be used as callback to indicate completion
     */
    void DeletePage(page_id_t page_id, std::promise<void>&& done);

    /*
     * Request to read a page
     *
     * Requires the following parameters:
     * 1. A page_id of the page to read from
     * 2. A mutable span to write the page data into
     * 3. A void promise to be used as a callback to indicate completion
     */
    void ReadPage(page_id_t page_id, MutPageView data, std::promise<bool>&& status);

    /*
     * Request to write to a page
     *
     * Requires the following parameters:
     * 1. A page_id of the page to read from
     * 2. A span of data to write into the page
     * 3. A bool promise to be used as a callback to indicate the success
     *    status of the request
     */
    void WritePage(page_id_t page_id, PageView data, std::promise<bool>&& status);

private:
    /*
     * Worker thread function to process each of the requests
     */
    void WorkerFunction(std::stop_token stop_token);

    /* thread-safe task queue with a condition variable to notify worker thread
     * of task arrival */
    std::condition_variable cv_m;
    std::mutex mut_m;
    std::queue<IOTasks::Task> tasks_m;

    /* underlying disk manager for worker thread to perform I/O operations */
    DiskManager disk_manager_m;

    std::shared_ptr<spdlog::logger> logger_m;

    std::jthread worker_thread_m;
};
