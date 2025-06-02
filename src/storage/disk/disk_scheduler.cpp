#include "storage/disk/disk_scheduler.h"
#include "storage/disk/disk_manager.h"
#include "storage/disk/io_tasks.h"
#include <stop_token>
#include <thread>

DiskScheduler::DiskScheduler(const std::filesystem::path& db_file) :
	disk_manager_m(db_file),
	worker_thread_m(&DiskScheduler::WorkerFunction, this)
{
}

DiskScheduler::~DiskScheduler()
{
	worker_thread_m.request_stop();
}

void DiskScheduler::WorkerFunction(std::stop_token stop_token)
{
	std::unique_lock<std::mutex> lk(mut_m);
	while (not stop_token.stop_requested())
	{
		cv_m.wait(lk, [this](){ return not tasks_m.empty(); });
		while (not tasks_m.empty())
		{
			IOTasks::Task task = std::move(tasks_m.front());		
			tasks_m.pop();
			lk.unlock();

			// Process everything here
			// TODO pass exceptions too
			switch (task.type) {
			case IOTasks::TaskType::CREATE_PAGE: {
				IOTasks::CreatePageData data = std::move(std::get<IOTasks::CreatePageData>(task.data));
				page_id_t page_id = disk_manager_m.AllocatePage();
				data.page_promise.set_value(page_id);
				break;
			}
			case IOTasks::TaskType::DELETE_PAGE: {
				IOTasks::DeletePageData data = std::move(std::get<IOTasks::DeletePageData>(task.data));
				disk_manager_m.DeletePage(data.page_id);
				data.completed_promise.set_value();
				break;
			}
			case IOTasks::TaskType::WRITE_PAGE: {
				IOTasks::WritePageData data = std::move(std::get<IOTasks::WritePageData>(task.data));
				disk_manager_m.WritePage(data.page_id, data.data);
				data.completed_promise.set_value();
				break;
			}
			case IOTasks::TaskType::READ_PAGE: {
				IOTasks::ReadPageData data = std::move(std::get<IOTasks::ReadPageData>(task.data));
				disk_manager_m.ReadPage(data.page_id, data.data);
				data.data_promise.set_value(data.data);
				break;
			}
			}

			lk.lock();
		}
	}
}


void DiskScheduler::Enqueue(const IOTasks::Task& task)
{
	std::lock_guard<std::mutex> lg(mut_m);
	tasks_m.push(task);
	cv_m.notify_one();
}
