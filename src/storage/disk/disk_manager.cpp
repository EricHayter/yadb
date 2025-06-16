#include "storage/disk/disk_manager.h"
#include "common/type_definitions.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <cassert>
#include <filesystem>

DiskManager::DiskManager(const std::filesystem::path& db_directory)
    : DiskManager(db_directory, 1)
{
}

DiskManager::DiskManager(const std::filesystem::path& db_directory, std::size_t page_capacity)
    : db_directory_m(db_directory / DB_FILE_NAME)
    , page_capacity_m(page_capacity)
{
    if (not std::filesystem::exists(db_directory)) {
        std::filesystem::create_directory(db_directory);
    }

    // create db file
    db_io_m.open(db_directory_m, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    assert(db_io_m.is_open());

    std::filesystem::resize_file(db_directory_m, GetDatabaseFileSize());
    assert(GetDatabaseFileSize() == std::filesystem::file_size(db_directory_m));
    for (page_id_t id = 0; id < page_capacity_m; id++) {
        free_pages_m.insert(id);
    }

    // create logger
    logger_m = spdlog::basic_logger_mt("disk_manager_logger", db_directory_m / DISK_MANAGER_LOG_FILE_NAME);
    logger_m->info("Successfully initialized disk manager");
}

DiskManager::~DiskManager()
{
    db_io_m.close();
    logger_m->info("Closed disk manager");
}

bool DiskManager::WritePage(page_id_t page_id, PageView page_data)
{
    assert(page_id < page_capacity_m && not free_pages_m.contains(page_id));
    std::size_t offset = GetOffset(page_id);
    db_io_m.seekg(offset);
    db_io_m.write(page_data.data(), page_data.size());
    if (not db_io_m.good()) {
        logger_m->warn("Failed to write data to page id {}", page_id);
        return false;
    }
    return true;
}

// Read the contents of page data into page_data
bool DiskManager::ReadPage(page_id_t page_id, MutPageView page_data)
{
    assert(page_id < page_capacity_m && not free_pages_m.contains(page_id));
    size_t offset = GetOffset(page_id);
    db_io_m.seekg(offset);
    db_io_m.read(page_data.data(), page_data.size());
    if (not db_io_m.good()) {
        logger_m->warn("Failed to read data from page id {}", page_id);
        return false;
    }
    return true;
}

void DiskManager::DeletePage(page_id_t page_id)
{
    free_pages_m.insert(page_id);
}

page_id_t DiskManager::AllocatePage()
{
    page_id_t page_id;
    if (not free_pages_m.empty()) {
        auto iter = free_pages_m.begin();
        page_id = *iter;
        free_pages_m.erase(iter);
    } else {
        page_id = page_capacity_m;
        if (page_capacity_m == 0) {
            page_capacity_m = 1;
        } else {
            page_capacity_m *= 2;
        }
        std::filesystem::resize_file(db_directory_m, GetDatabaseFileSize());

        // populate free page list with new pages
        for (int id = page_id + 1; id < page_capacity_m; id++)
            free_pages_m.insert(id);
    }
    return page_id;
}

std::size_t DiskManager::GetOffset(page_id_t page_id)
{
    return page_id * PAGE_SIZE;
}

std::size_t DiskManager::GetDatabaseFileSize()
{
    return page_capacity_m * PAGE_SIZE;
}
