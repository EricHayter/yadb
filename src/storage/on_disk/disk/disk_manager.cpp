#include "storage/on_disk/disk/disk_manager.h"
#include "storage/on_disk/constants.h"
#include "storage/on_disk/disk/disk_manager_error.h"
#include "storage/on_disk/types.h"
#include "core/assert.h"
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <random>

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

std::optional<yadb::Error::Ptr> DiskManager::WritePage(const file_page_id_t& fp_id, FullPage page)
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
    if (!db_file.file_stream.good())
        return std::make_shared<yadb::PageWriteError>(fp_id, "stream failure");
    return std::nullopt;
}

std::optional<yadb::Error::Ptr> DiskManager::ReadPage(const file_page_id_t& fp_id, MutFullPage page)
{
    std::unique_lock<std::mutex> lk(mut_m);
    DatabaseFile& db_file = OpenFile(fp_id.file_id);
    lk.unlock();

    std::lock_guard<std::mutex> lg(*db_file.mut);
    YADB_ASSERT(fp_id.page_id < db_file.page_capacity && !db_file.free_pages.contains(fp_id.page_id), "Out of index page");
    size_t offset = GetOffset(fp_id.page_id);
    db_file.file_stream.seekg(static_cast<std::streamoff>(offset));
    db_file.file_stream.read(reinterpret_cast<char*>(page.data()), page.size());
    if (!db_file.file_stream.good())
        return std::make_shared<yadb::PageReadError>(fp_id, "stream failure");
    return std::nullopt;
}

std::optional<yadb::Error::Ptr> DiskManager::DeletePage(const file_page_id_t& fp_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    DatabaseFile& db_file = OpenFile(fp_id.file_id);
    lk.unlock();

    std::lock_guard<std::mutex> lg(*db_file.mut);
    YADB_ASSERT(fp_id.page_id < db_file.page_capacity && !db_file.free_pages.contains(fp_id.page_id), "Out of index page");
    db_file.free_pages.insert(fp_id.page_id);
    return std::nullopt;
}

std::expected<file_id_t, yadb::Error::Ptr> DiskManager::CreateFile()
{
    std::lock_guard<std::mutex> lg(mut_m);

    static thread_local std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<file_id_t> dis(0, std::numeric_limits<file_id_t>::max());

    for (uint32_t i = 0; i < MAX_FILE_ID_RETRIES; i++) {
        file_id_t file_id = dis(gen);
        if (IsReservedFileId(file_id))
            continue;
        std::filesystem::path file_path = GetFilePath(file_id);
        if (!std::filesystem::exists(file_path)) {
            std::ofstream fstream(file_path);
            return file_id;
        }
    }
    return std::unexpected(std::make_shared<yadb::CreateFileError>(INVALID_FILE_ID, "exhausted retries generating unique file id"));
}

std::optional<yadb::Error::Ptr> DiskManager::CreateFile(file_id_t file_id)
{
    std::lock_guard<std::mutex> lg(mut_m);
    std::filesystem::path file_path = GetFilePath(file_id);
    if (std::filesystem::exists(file_path))
        return std::make_shared<yadb::CreateFileError>(file_id, "file already exists");
    std::ofstream fstream(file_path);
    return std::nullopt;
}

std::optional<yadb::Error::Ptr> DiskManager::DeleteFile(file_id_t file_id)
{
    std::lock_guard<std::mutex> lg(mut_m);

    if (id_map_m.contains(file_id)) {
        id_map_m[file_id].file_stream.close();
        id_map_m.erase(file_id);
    }

    std::error_code ec;
    std::filesystem::remove(GetFilePath(file_id), ec);
    if (ec)
        return std::make_shared<yadb::DeleteFileError>(file_id, ec.message());
    return std::nullopt;
}

DiskManager::DatabaseFile& DiskManager::OpenFile(file_id_t file_id)
{
    if (id_map_m.contains(file_id))
        return id_map_m[file_id];

    std::filesystem::path file_path = GetFilePath(file_id);
    id_map_m.emplace(file_id,
        DatabaseFile{
            .mut = std::make_unique<std::mutex>(),
            .path = file_path,
            .file_stream = std::fstream(file_path),
            .free_pages = std::unordered_set<page_id_t>(),
            .page_capacity = 1 // TODO this isn't correct...
        });

    return id_map_m[file_id];
}

std::expected<page_id_t, yadb::Error::Ptr> DiskManager::AllocatePage(file_id_t file_id)
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
        std::error_code ec;
        std::filesystem::resize_file(db_file.path, db_file.page_capacity * PAGE_SIZE, ec);
        if (ec)
            return std::unexpected(std::make_shared<yadb::AllocatePageError>(file_id, ec.message()));

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
