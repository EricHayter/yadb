#include "storage/on_disk/disk/disk_manager.h"
#include "common/definitions.h"
#include "config/config.h"
#include "spdlog/fmt/bundled/base.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/logger.h"
#include <cassert>
#include <stdexcept>
#include <filesystem>
#include "core/assert.h"

DiskManager::DiskManager()
    : DiskManager(DatabaseConfig::CreateNull())
{
}

DiskManager::DiskManager(const DatabaseConfig& config)
    : logger_m(config.disk_manager_logger)
{
}

DiskManager::~DiskManager()
{
    for (auto& [file_id, file_info] : id_map_m) {
        if (file_info.file_stream.is_open()) {
            file_info.file_stream.close();
        }
    }
}

file_id_t DiskManager::RegisterFile(const std::filesystem::path& file_path, std::size_t page_capacity)
{
    if (path_map_m.contains(file_path))
        return path_map_m[file_path];

    file_id_t file_id = GenerateFileId();
    path_map_m[file_path] = file_id;
    DatabaseFile& file_info = id_map_m[file_id];
    file_info.path = file_path;

    bool file_exists = std::filesystem::exists(file_path);

    if (file_exists) {
        // Open existing file
        file_info.file_stream.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
        file_info.page_capacity = std::filesystem::file_size(file_path) / PAGE_SIZE;
        // TODO: Load free_pages from file metadata (maybe from page 0?)
    } else {
        // Create new file
        file_info.file_stream.open(file_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        file_info.page_capacity = page_capacity;
        std::filesystem::resize_file(file_path, file_info.page_capacity * PAGE_SIZE);
        for (page_id_t id = 0; id < file_info.page_capacity; id++) {
            file_info.free_pages.insert(id);
        }
    }

    return file_id;
}

file_id_t DiskManager::GenerateFileId()
{
    return next_file_id_m++;
}

bool DiskManager::WritePage(file_id_t file_id, page_id_t page_id, FullPage page)
{
    YADB_ASSERT(id_map_m.contains(file_id), "Page hasn't been registered");
    DatabaseFile& file_info = id_map_m[file_id];

    YADB_ASSERT(page_id < file_info.page_capacity && !file_info.free_pages.contains(page_id), "Out of index page");
    std::size_t offset = GetOffset(page_id);
    file_info.file_stream.seekg(offset);

    file_info.file_stream.write(reinterpret_cast<const char*>(page.data()), page.size());
    file_info.file_stream.flush();
    if (!file_info.file_stream.good()) {
        logger_m->warn("Failed to write data to page id {}", page_id);
        return false;
    }
    return true;
}

// Read the contents of page data into page_data
bool DiskManager::ReadPage(file_id_t file_id, page_id_t page_id, MutFullPage page)
{
    YADB_ASSERT(id_map_m.contains(file_id), "Page hasn't been registered");
    DatabaseFile& file_info = id_map_m[file_id];

    YADB_ASSERT(page_id < file_info.page_capacity && !file_info.free_pages.contains(page_id), "Out of index page");

    size_t offset = GetOffset(page_id);
    file_info.file_stream.seekg(offset);
    file_info.file_stream.read(reinterpret_cast<char*>(page.data()), page.size());
    if (!file_info.file_stream.good()) {
        logger_m->warn("Failed to read data from page id {}", page_id);
        return false;
    }
    return true;
}

void DiskManager::DeletePage(file_id_t file_id, page_id_t page_id)
{
    YADB_ASSERT(id_map_m.contains(file_id), "Page hasn't been registered");
    DatabaseFile& file_info = id_map_m[file_id];

    YADB_ASSERT(page_id < file_info.page_capacity && !file_info.free_pages.contains(page_id), "Out of index page");
    file_info.free_pages.insert(page_id);
}

page_id_t DiskManager::AllocatePage(file_id_t file_id)
{
    YADB_ASSERT(id_map_m.contains(file_id), "Page hasn't been registered");
    DatabaseFile& file_info = id_map_m[file_id];

    page_id_t page_id;
    if (!file_info.free_pages.empty()) {
        auto iter = file_info.free_pages.begin();
        page_id = *iter;
        file_info.free_pages.erase(iter);
    } else {
        page_id = file_info.page_capacity;
        if (file_info.page_capacity == 0) {
            file_info.page_capacity = 1;
        } else {
            file_info.page_capacity *= 2;
        }
        std::filesystem::resize_file(file_info.path, GetDatabaseFileSize(file_id));

        // populate free page list with new pages
        for (int id = page_id + 1; id < file_info.page_capacity; id++)
            file_info.free_pages.insert(id);
    }
    return page_id;
}

std::size_t DiskManager::GetOffset(page_id_t page_id) const
{
    return page_id * PAGE_SIZE;
}

std::size_t DiskManager::GetDatabaseFileSize(file_id_t file_id) const
{
    YADB_ASSERT(id_map_m.contains(file_id), "Page hasn't been registered");
    const DatabaseFile& file_info = id_map_m.at(file_id);
    return file_info.page_capacity * PAGE_SIZE;
}
