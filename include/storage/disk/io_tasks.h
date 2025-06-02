#pragma once
#include "common/type_definitions.h"
#include <future>
#include <variant>

namespace IOTasks {

enum class TaskType {
	CREATE_PAGE,
	DELETE_PAGE,
	WRITE_PAGE,
	READ_PAGE,
};

struct CreatePageData {
	std::promise<page_id_t> page_promise;
};

struct DeletePageData {
	page_id_t page_id;
	std::promise<void> completed_promise;
};

struct WritePageData {
	page_id_t page_id;
	PageData data;
	std::promise<void> completed_promise;
};

struct ReadPageData {
	page_id_t page_id;
	PageData data;
	std::promise<PageData> data_promise;
};

struct Task {
	TaskType type;
	std::variant<CreatePageData, DeletePageData, WritePageData, ReadPageData> data;
};
}; // namespace IOTasks
