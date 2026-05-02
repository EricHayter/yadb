#include "storage/on_disk/disk/disk_manager.h"
#include "storage/on_disk/constants.h"
#include "storage/on_disk/types.h"
#include "config/config.h"
#include "spdlog/fmt/bundled/base.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/logger.h"
#include <cassert>
#include <cstdlib>
#include <mutex>
#include <ctime>
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

std::filesystem::path DiskManager::GetFilePath(file_id_t file_id)
{
    return std::to_string(file_id) + ".yadb";
}

bool DiskManager::WritePage(const file_page_id_t& fp_id, FullPage page)
{
    std::unique_lock<std::mutex> lk(mut_m);
    DatabaseFile& db_file = OpenFile(fp_id.file_id);
    lk.unlock();

    std::lock_guard<std::mutex> lg(*db_file.mut);
    YADB_ASSERT(fp_id.page_id < db_file.page_capacity && !db_file.free_pages.contains(fp_id.page_id), "Out of index page");
    std::size_t offset = GetOffset(fp_id.page_id);
    db_file.file_stream.seekp(static_cast<std::streamoff>(offset));

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
    DatabaseFile& db_file = OpenFile(fp_id.file_id);
    lk.unlock();

    std::lock_guard<std::mutex> lg(*db_file.mut);
    YADB_ASSERT(fp_id.page_id < db_file.page_capacity && !db_file.free_pages.contains(fp_id.page_id), "Out of index page");

    size_t offset = GetOffset(fp_id.page_id);
    db_file.file_stream.seekg(static_cast<std::streamoff>(offset));
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
    DatabaseFile& db_file = OpenFile(fp_id.file_id);
    lk.unlock();

    std::lock_guard<std::mutex> lg(*db_file.mut);
    YADB_ASSERT(fp_id.page_id < db_file.page_capacity && !db_file.free_pages.contains(fp_id.page_id), "Out of index page");
    db_file.free_pages.insert(fp_id.page_id);
}

file_id_t DiskManager::CreateFile()
{
    std::lock_guard<std::mutex> lg(mut_m);

    // seed std::rand
    std::srand(static_cast<unsigned int>(std::time({})));
    for (uint32_t i = 0; i < MAX_FILE_ID_RETRIES; i++) {
        file_id_t file_id = static_cast<file_id_t>(std::rand());

        // Skip reserved catalog file IDs
        if (IsReservedFileId(file_id)) {
            continue;
        }

        std::filesystem::path file_path = GetFilePath(file_id);

        // file doesn't exist so this file id is unique!
        if (!std::filesystem::exists(file_path)) {
            // create the file
            std::ofstream fstream(file_path);
            return file_id;
        }
    }
    throw std::runtime_error("Couldn't generate a unique file id");
}

void DiskManager::CreateFile(file_id_t file_id)
{
    std::lock_guard<std::mutex> lg(mut_m);
    std::filesystem::path file_path = GetFilePath(file_id);

    if (std::filesystem::exists(file_path)) {
        throw std::runtime_error("File with id " + std::to_string(file_id) + " already exists");
    }

    // create the file
    std::ofstream fstream(file_path);
}

DiskManager::DatabaseFile& DiskManager::OpenFile(file_id_t  file_id)
{

    // fast path: already have the file cached.
    if (id_map_m.contains(file_id)) {
        return id_map_m[file_id];
    }

    // This will fail if we have a bunch of files open...
    std::filesystem::path file_path = GetFilePath(file_id);
    id_map_m.emplace(file_id,
        DatabaseFile{
            .mut = std::make_unique<std::mutex>(),
            .path = file_path,
            .file_stream = std::fstream(file_path),
            .free_pages = std::unordered_set<page_id_t>(),
            .page_capacity = 1 // TODO this isn't correct...
        }
    );

    return id_map_m[file_id];
}

page_id_t DiskManager::AllocatePage(file_id_t file_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    DatabaseFile& db_file = OpenFile(file_id);
    lk.unlock();

    std::lock_guard<std::mutex> lg(*db_file.mut);
    page_id_t page_id;
    if (!db_file.free_pages.empty()) {
        auto iter = db_file.free_pages.begin();
        page_id = *iter;
        db_file.free_pages.erase(iter);
    } else {
        page_id = static_cast<page_id_t>(db_file.page_capacity);
        if (db_file.page_capacity == 0) {
            db_file.page_capacity = 1;
        } else {
            db_file.page_capacity *= 2;
        }
        std::filesystem::resize_file(db_file.path, db_file.page_capacity * PAGE_SIZE);

        // populate free page list with new pages
        for (page_id_t id = page_id + 1; id < db_file.page_capacity; id++)
            db_file.free_pages.insert(id);
    }
    return page_id;
}

std::size_t DiskManager::GetOffset(page_id_t page_id) const
{
    return page_id * PAGE_SIZE;
}

std::size_t DiskManager::GetDatabaseFileSize(file_id_t file_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    const DatabaseFile& db_file = OpenFile(file_id);
    lk.unlock();

    std::lock_guard<std::mutex> lg(*db_file.mut);
    return db_file.page_capacity * PAGE_SIZE;
}
