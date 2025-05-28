#pragma once
#include "common/type_definitions.h"
#include <future>
#include <queue>
#include <optional>
#include <thread>
#include <condition_variable>

struct DiskIOTask
{
	bool is_write;
	PageData data;
	page_id_t page_id;
	std::future<bool> result; // maybe change this to a enum
};

class DiskScheduler
{
	public:
	DiskScheduler();
	~DiskScheduler();
	void Enqueue(const DiskIOTask& task);

	private:
	std::thread worker_thread_m;	

	std::condition_variable cv_m;
	std::mutex mut_m;
	std::queue<DiskIOTask> tasks_m;

	// TODO
};
