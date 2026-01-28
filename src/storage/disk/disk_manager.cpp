#include "storage/disk/disk_manager.h"
#include <cassert>
#include <filesystem>
#include "config/config.h"
#include "spdlog/fmt/bundled/base.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/logger.h"

DiskManager::DiskManager()
    : DiskManager(128)
{
}

DiskManager::DiskManager(std::size_t page_capacity)
    : DiskManager(DatabaseConfig::CreateNull(), page_capacity)
{
}

DiskManager::DiskManager(const DatabaseConfig& config, std::size_t page_capacity = 128)
    : db_file_path_m(config.database_file)
    , logger_m(config.disk_manager_logger)
    , page_capacity_m(page_capacity)
{
    // create db file
    db_io_m.open(db_file_path_m, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    assert(db_io_m.is_open());

    std::filesystem::resize_file(db_file_path_m, GetDatabaseFileSize());
    assert(GetDatabaseFileSize() == std::filesystem::file_size(db_file_path_m));
    for (page_id_t id = 0; id < page_capacity_m; id++) {
        free_pages_m.insert(id);
    }
}

DiskManager::~DiskManager()
{
    db_io_m.close();
}

bool DiskManager::WritePage(page_id_t page_id, FullPage page)
{
    assert(page_id < page_capacity_m && !free_pages_m.contains(page_id));
    std::size_t offset = GetOffset(page_id);
    db_io_m.seekg(offset);

    db_io_m.write(reinterpret_cast<const char*>(page.data()), page.size());
    if (!db_io_m.good()) {
        logger_m->warn("Failed to write data to page id {}", page_id);
        return false;
    }
    return true;
}

// Read the contents of page data into page_data
bool DiskManager::ReadPage(page_id_t page_id, MutFullPage page)
{
    assert(page_id < page_capacity_m && !free_pages_m.contains(page_id));
    size_t offset = GetOffset(page_id);
    db_io_m.seekg(offset);
    db_io_m.read(reinterpret_cast<char*>(page.data()), page.size());
    if (!db_io_m.good()) {
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
    if (!free_pages_m.empty()) {
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
        std::filesystem::resize_file(db_file_path_m, GetDatabaseFileSize());

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
