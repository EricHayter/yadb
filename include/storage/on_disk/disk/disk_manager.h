#pragma once

#include "common/definitions.h"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_set>
#include <unordered_map>
namespace spdlog {
class logger;
}
struct DatabaseConfig;

/*
 * Disk manager class to handle the database file
 *
 * This class provides the functionality for the disk manager which handles
 * all associated I/O operations required loading, and writing to disk for the
 * page buffer manager. This class also provides additional functionality
 * for allocating new pages inside of the database file.
 */
class DiskManager {
public:
    DiskManager();
    DiskManager(const DatabaseConfig& config);
    ~DiskManager();

    file_id_t RegisterFile(const std::filesystem::path& file_path, std::size_t page_capacity);

    /*
     * Allocated a new page in the database file for writing to
     *
     * This function will either a) use a "free" page already existing in the
     * database file or b) increase the capacity of the database file.
     */
    page_id_t AllocatePage(file_id_t file_id);

    /*
     * Write page data to disk
     *
     * return true on success false otherwise.
     */
    bool WritePage(file_id_t file_id, page_id_t page_id, FullPage page);

    /*
     * Read data from disk
     *
     * return true on success false otherwise.
     */
    bool ReadPage(file_id_t file_id, page_id_t page_id, MutFullPage page);

    /*
     * Deletes a page from the database file
     *
     * NOTE: this only LOGICALLY deletes the page from the database file. The
     * data is in fact still there and there is no shrinkage of the database
     * file itself. The page may then reused when allocating new pages.
     */
    void DeletePage(file_id_t, page_id_t page_id);

private:
    file_id_t GenerateFileId();
    std::size_t GetOffset(page_id_t page_id) const;
    std::size_t GetDatabaseFileSize(file_id_t file_id) const;

    struct DatabaseFile {
        std::filesystem::path path;

        /* iostream to write to database file */
        std::fstream file_stream;

        /* list of pages that are considered free */
        std::unordered_set<page_id_t> free_pages;
        std::size_t page_capacity = 1;
    };

    std::unordered_map<std::filesystem::path, file_id_t> path_map_m;
    std::unordered_map<file_id_t, DatabaseFile> id_map_m;

    file_id_t next_file_id_m = 0;

    std::shared_ptr<spdlog::logger> logger_m;
};
