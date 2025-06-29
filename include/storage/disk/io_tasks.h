#pragma once
#include "common/type_definitions.h"
#include <future>
#include <variant>

namespace IOTasks {

/**
 * @brief Struct to represent a task for creating a page
 */
struct AllocatePageTask {
    std::promise<page_id_t> result;
};

/**
 * @brief Struct to represent a task for deleting a page
 */
struct DeletePageTask {
    page_id_t page_id;
    std::promise<void> done;
};

/**
 * @brief Struct to represent a task for writing to a page
 */
struct WritePageTask {
    page_id_t page_id;
    PageView data; /// span containing data to write to page
    std::promise<bool> status;
};

/**
 * @brief Struct to represent a task for reading from a page
 */
struct ReadPageTask {
    page_id_t page_id;
    MutPageView data; /// span to write data from read into
    std::promise<bool> status;
};

using Task = std::variant<AllocatePageTask, DeletePageTask, WritePageTask, ReadPageTask>;

}; // namespace IOTasks
