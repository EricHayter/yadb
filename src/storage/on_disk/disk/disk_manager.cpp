#include "storage/on_disk/disk/disk_manager.h"
#include "common/definitions.h"
#include "config/config.h"
#include "spdlog/fmt/bundled/base.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/logger.h"
#include <cassert>
#include <mutex>
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
    for (auto& [file_id, db_file] : id_map_m) {
        if (db_file.file_stream.is_open()) {
            db_file.file_stream.close();
        }
    }
}

file_id_t DiskManager::RegisterFile(const std::filesystem::path& file_path, std::size_t page_capacity)
{
    std::lock_guard<std::mutex> lk(mut_m);
    if (path_map_m.contains(file_path))
        return path_map_m[file_path];

    file_id_t file_id = GenerateFileId();
    path_map_m[file_path] = file_id;
    DatabaseFile& db_file = id_map_m[file_id];

    db_file.path = file_path;

    bool file_exists = std::filesystem::exists(file_path);

    if (file_exists) {
        // Open existing file
        db_file.file_stream.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
        db_file.page_capacity = std::filesystem::file_size(file_path) / PAGE_SIZE;
        // TODO: Load free_pages from file metadata (maybe from page 0?)
    } else {
        // Create new file
        db_file.file_stream.open(file_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        db_file.page_capacity = page_capacity;
        std::filesystem::resize_file(file_path, db_file.page_capacity * PAGE_SIZE);
        for (page_id_t id = 0; id < db_file.page_capacity; id++) {
            db_file.free_pages.insert(id);
        }
    }

    return file_id;
}

file_id_t DiskManager::GenerateFileId()
{
    // NOTE: Caller must hold mut_m lock
    return next_file_id_m++;
}

bool DiskManager::WritePage(const file_page_id_t& fp_id, FullPage page)
{
    std::unique_lock<std::mutex> lk(mut_m);
    YADB_ASSERT(id_map_m.contains(fp_id.file_id), "File hasn't been registered");
    DatabaseFile& db_file = id_map_m[fp_id.file_id];
    lk.unlock();

    std::lock_guard<std::mutex> lg(db_file.mut);
    YADB_ASSERT(fp_id.page_id < db_file.page_capacity && !db_file.free_pages.contains(fp_id.page_id), "Out of index page");
    std::size_t offset = GetOffset(fp_id.page_id);
    db_file.file_stream.seekp(offset);

    db_file.file_stream.write(reinterpret_cast<const char*>(page.data()), page.size());
    db_file.file_stream.flush();
    if (!db_file.file_stream.good()) {
        logger_m->warn("Failed to write data to page id {}", fp_id.page_id);
        return false;
    }
    return true;
}

// Read the contents of page data into page_data
bool DiskManager::ReadPage(const file_page_id_t& fp_id, MutFullPage page)
{
    std::unique_lock<std::mutex> lk(mut_m);
    YADB_ASSERT(id_map_m.contains(fp_id.file_id), "File hasn't been registered");
    DatabaseFile& db_file = id_map_m[fp_id.file_id];
    lk.unlock();

    std::lock_guard<std::mutex> lg(db_file.mut);
    YADB_ASSERT(fp_id.page_id < db_file.page_capacity && !db_file.free_pages.contains(fp_id.page_id), "Out of index page");

    size_t offset = GetOffset(fp_id.page_id);
    db_file.file_stream.seekg(offset);
    db_file.file_stream.read(reinterpret_cast<char*>(page.data()), page.size());
    if (!db_file.file_stream.good()) {
        logger_m->warn("Failed to read data from page id {}", fp_id.page_id);
        return false;
    }
    return true;
}

void DiskManager::DeletePage(const file_page_id_t& fp_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    YADB_ASSERT(id_map_m.contains(fp_id.file_id), "File hasn't been registered");
    DatabaseFile& db_file = id_map_m[fp_id.file_id];
    lk.unlock();

    std::lock_guard<std::mutex> lg(db_file.mut);
    YADB_ASSERT(fp_id.page_id < db_file.page_capacity && !db_file.free_pages.contains(fp_id.page_id), "Out of index page");
    db_file.free_pages.insert(fp_id.page_id);
}

page_id_t DiskManager::AllocatePage(file_id_t file_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    YADB_ASSERT(id_map_m.contains(file_id), "File hasn't been registered");
    DatabaseFile& db_file = id_map_m[file_id];
    lk.unlock();

    std::lock_guard<std::mutex> lg(db_file.mut);
    page_id_t page_id;
    if (!db_file.free_pages.empty()) {
        auto iter = db_file.free_pages.begin();
        page_id = *iter;
        db_file.free_pages.erase(iter);
    } else {
        page_id = db_file.page_capacity;
        if (db_file.page_capacity == 0) {
            db_file.page_capacity = 1;
        } else {
            db_file.page_capacity *= 2;
        }
        std::filesystem::resize_file(db_file.path, db_file.page_capacity * PAGE_SIZE);

        // populate free page list with new pages
        for (int id = page_id + 1; id < db_file.page_capacity; id++)
            db_file.free_pages.insert(id);
    }
    return page_id;
}

std::size_t DiskManager::GetOffset(page_id_t page_id) const
{
    return page_id * PAGE_SIZE;
}

std::size_t DiskManager::GetDatabaseFileSize(file_id_t file_id) const
{
    std::unique_lock<std::mutex> lk(mut_m);
    YADB_ASSERT(id_map_m.contains(file_id), "File hasn't been registered");
    const DatabaseFile& db_file = id_map_m.at(file_id);
    lk.unlock();

    std::lock_guard<std::mutex> lg(db_file.mut);
    return db_file.page_capacity * PAGE_SIZE;
}
