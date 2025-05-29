#include "storage/disk/disk_scheduler.h"
#include <mutex>
#include <thread>

DiskScheduler::DiskScheduler()
{
	worker_thread_m = std::jthread(&DiskScheduler::WorkerFunction, this);
}

DiskScheduler::~DiskScheduler()
{
	worker_thread_m.request_stop();
}


void DiskScheduler::Enqueue(const DiskIOTask& task)
{
	std::lock_guard<std::mutex> lg(mut_m);
	tasks_m.push(task);
	cv_m.notify_one();
}


void DiskScheduler::WorkerFunction(std::stop_token st)
{
	while (not st.stop_requested())
	{
		std::unique_lock<std::mutex> lk(mut_m);
		cv_m.wait(lk, [this](){ return not tasks_m.empty(); });

		while (not tasks_m.empty())
		{
			DiskIOTask task = std::move(tasks_m.front());
			tasks_m.pop();
			lk.unlock();

			if (task.is_write)
			{
				disk_manager_m.WritePage(task.page_id, task.data);
			}
			else
			{
				disk_manager_m.ReadPage(task.page_id, task.data);
			}

			// TODO this should have much better error handling
			// give an enum instead of bool
			task.callback.set_value(true);
			lk.lock();
		}
	}
}
