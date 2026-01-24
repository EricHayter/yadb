#pragma once

#include <future>
#include <variant>

#include "storage/bptree/page/page.h"

/*
 * Disk Scheduler Tasks
 *
 * For each of the different tasks supported by the disk scheduler a struct
 * is created containing all of the required fields for representing the
 * parameters and output of the request.
 */
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
    FullPage data;
    std::promise<bool> status;
};

struct ReadPageTask {
    page_id_t page_id;
    MutFullPage data;
    std::promise<bool> status;
};

using Task = std::variant<AllocatePageTask, DeletePageTask, WritePageTask, ReadPageTask>;

}; // namespace IOTasks
