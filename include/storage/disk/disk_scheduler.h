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

class DiskScheduler {
public:
    DiskScheduler(const std::filesystem::path& db_file);
    ~DiskScheduler();

    void AllocatePage(std::promise<page_id_t>&& result);
    void DeletePage(page_id_t page_id, std::promise<void>&& done);
    void ReadPage(page_id_t page_id, MutPageView data, std::promise<void>&& done);
    void WritePage(page_id_t page_id, PageView data, std::promise<void>&& done);

private:
    void WorkerFunction(std::stop_token stop_token);
    std::jthread worker_thread_m;
    std::condition_variable cv_m;
    std::mutex mut_m;
    std::queue<IOTasks::Task> tasks_m;
    DiskManager disk_manager_m;
};
