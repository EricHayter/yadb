#include "storage/disk/disk_scheduler.h"

DiskScheduler::DiskScheduler()
{
	auto worker_function = [this](){
		std::unique_lock<std::mutex> lk(mut_m);
		while (true) {
			cv_m.wait(lk, [this](){ return not tasks_m.empty(); });
			
			// we have an element now
			DiskIOTask &task = tasks_m.front();

			// perform the logic here

			tasks_m.pop();
			lk.unlock();
		}
	};

}


void DiskScheduler::Enqueue(const DiskIOTask& task)
{
	std::lock_guard<std::mutex> lg(mut_m);
	tasks_m.push(task);
	cv_m.notify_one();
}


std::optional<DiskIOTask> DiskScheduler::Deqeue()
{
}
