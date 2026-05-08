#pragma once

#include "storage/on_disk/types.h"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <mutex>

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

    /*
     * Creates a new file and returns the corresponding file_id_t.
     */
    file_id_t CreateFile();

    /*
     * Creates a new file with the specified file_id.
     */
    void CreateFile(file_id_t file_id);

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
    bool WritePage(const file_page_id_t& fp_id, FullPage page);

    /*
     * Read data from disk
     *
     * return true on success false otherwise.
     */
    bool ReadPage(const file_page_id_t& fp_id, MutFullPage page);

    /*
     * Deletes a page from the database file
     *
     * NOTE: this only LOGICALLY deletes the page from the database file. The
     * data is in fact still there and there is no shrinkage of the database
     * file itself. The page may then reused when allocating new pages.
     */
    void DeletePage(const file_page_id_t& fp_id);

    /*
     * Closes and removes the physical file for the given file_id.
     */
    void DeleteFile(file_id_t file_id);

private:
    struct DatabaseFile {
        mutable std::unique_ptr<std::mutex> mut;
        std::filesystem::path path;

        /* iostream to write to database file */
        std::fstream file_stream;

        /* list of pages that are considered free */
        std::unordered_set<page_id_t> free_pages;
        std::size_t page_capacity = 1;
    };

    std::unordered_map<file_id_t, DatabaseFile> id_map_m;
    DatabaseFile& OpenFile(file_id_t file_id);

    static std::filesystem::path GetFilePath(file_id_t file_id);
    std::size_t GetOffset(page_id_t page_id) const;
    std::size_t GetDatabaseFileSize(file_id_t file_id);

    mutable std::mutex mut_m;

    std::shared_ptr<spdlog::logger> logger_m;
};
