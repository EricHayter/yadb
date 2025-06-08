#pragma once
#include "common/type_definitions.h"
#include <future>
#include <variant>

namespace IOTasks {

struct AllocatePageTask {
	std::promise<page_id_t> result;
};

struct DeletePageTask {
	page_id_t page_id;
	std::promise<void> done;
};

struct WritePageTask {
	page_id_t page_id;
	PageView data;
	std::promise<void> done;
};

struct ReadPageTask {
	page_id_t page_id;
	MutPageView data;
	std::promise<void> done;
};

using Task = std::variant<AllocatePageTask, DeletePageTask, WritePageTask, ReadPageTask>;

}; // namespace IOTasks
