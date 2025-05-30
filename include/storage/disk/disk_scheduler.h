#pragma once
#include "common/type_definitions.h"
#include "storage/disk/disk_manager.h"
#include <condition_variable>
#include <filesystem>
#include <future>
#include <optional>
#include <queue>
#include <stop_token>
#include <thread>

struct DiskIOTask {
    bool is_write;
    PageData data;
    page_id_t page_id;
    std::promise<bool> callback; // maybe change this to a enum
};

class DiskScheduler {
public:
    DiskScheduler(const std::filesystem::path& db_path);
    ~DiskScheduler();
    void Enqueue(const DiskIOTask& task);

private:
    void WorkerFunction(std::stop_token st);

    std::jthread worker_thread_m;
    std::condition_variable cv_m;
    std::mutex mut_m;
    std::queue<DiskIOTask> tasks_m;
    DiskManager disk_manager_m;
};
