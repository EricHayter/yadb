#include "storage/disk/disk_scheduler.h"
#include "storage/disk/disk_manager.h"
#include "storage/disk/io_tasks.h"
#include <mutex>
#include <stop_token>
#include <thread>

DiskScheduler::DiskScheduler(const std::filesystem::path& db_directory)
    : disk_manager_m(db_directory)
    , worker_thread_m(&DiskScheduler::WorkerFunction, this)
{
}

DiskScheduler::~DiskScheduler()
{
    worker_thread_m.request_stop();
    cv_m.notify_one();
}

void DiskScheduler::AllocatePage(std::promise<page_id_t>&& result)
{
    IOTasks::AllocatePageTask task {
        std::move(result),
    };

    std::lock_guard<std::mutex> lk(mut_m);
    tasks_m.push(std::move(task));
    cv_m.notify_one();
}

void DiskScheduler::DeletePage(page_id_t page_id, std::promise<void>&& done)
{
    IOTasks::DeletePageTask task {
        page_id,
        std::move(done),
    };

    std::lock_guard<std::mutex> lk(mut_m);
    tasks_m.push(std::move(task));
    cv_m.notify_one();
}

void DiskScheduler::ReadPage(page_id_t page_id, MutPageView data, std::promise<bool>&& status)
{
    IOTasks::ReadPageTask task {
        page_id,
        data,
        std::move(status),
    };

    std::lock_guard<std::mutex> lk(mut_m);
    tasks_m.push(std::move(task));
    cv_m.notify_one();
}

void DiskScheduler::WritePage(page_id_t page_id, PageView data, std::promise<bool>&& status)
{
    IOTasks::WritePageTask task {
        page_id,
        data,
        std::move(status),
    };

    std::lock_guard<std::mutex> lk(mut_m);
    tasks_m.push(std::move(task));
    cv_m.notify_one();
}

void DiskScheduler::WorkerFunction(std::stop_token stop_token)
{
    while (not stop_token.stop_requested()) {
        std::unique_lock<std::mutex> lk(mut_m);
        cv_m.wait(lk, [this, &stop_token]() {
            return not tasks_m.empty() || stop_token.stop_requested();
        });

        while (not tasks_m.empty()) {
            IOTasks::Task task = std::move(tasks_m.front());
            tasks_m.pop();
            lk.unlock();

            // Process the task accordingly
            std::visit([this](auto&& task) {
                using namespace IOTasks;
                using T = std::decay_t<decltype(task)>;
                if constexpr (std::is_same_v<T, AllocatePageTask>) {
                    page_id_t page_id = disk_manager_m.AllocatePage();
                    task.result.set_value(page_id);
                } else if constexpr (std::is_same_v<T, IOTasks::DeletePageTask>) {
                    disk_manager_m.DeletePage(task.page_id);
                    task.done.set_value();
                } else if constexpr (std::is_same_v<T, IOTasks::ReadPageTask>) {
                    task.status.set_value(disk_manager_m.ReadPage(task.page_id, task.data));
                } else if constexpr (std::is_same_v<T, IOTasks::WritePageTask>) {
                    task.status.set_value(disk_manager_m.WritePage(task.page_id, task.data));
                }
            },
                std::move(task));

            lk.lock();
        }
    }
}
