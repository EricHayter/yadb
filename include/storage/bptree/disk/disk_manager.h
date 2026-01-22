#pragma once

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_set>

#include "config/config.h"
#include "storage/bptree/page/page.h"

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
    DiskManager(std::size_t page_capacity);
    DiskManager(const DatabaseConfig& config, std::size_t page_capacity);
    ~DiskManager();

    /*
     * Allocated a new page in the database file for writing to
     *
     * This function will either a) use a "free" page already existing in the
     * database file or b) increase the capacity of the database file.
     */
    page_id_t AllocatePage();

    /*
     * Write page data to disk
     *
     * return true on success false otherwise.
     */
    bool WritePage(page_id_t page_id, PageView page);

    /*
     * Read data from disk
     *
     * return true on success false otherwise.
     */
    bool ReadPage(page_id_t page_id, MutPageView page);

    /*
     * Deletes a page from the database file
     *
     * NOTE: this only LOGICALLY deletes the page from the database file. The
     * data is in fact still there and there is no shrinkage of the database
     * file itself. The page may then reused when allocating new pages.
     */
    void DeletePage(page_id_t page_id);

private:
    std::size_t GetOffset(page_id_t page_id);
    std::size_t GetDatabaseFileSize();

    /* list of pages that are considered free */
    std::unordered_set<page_id_t> free_pages_m;

    std::size_t page_capacity_m;

    /* iostream to write to database file */
    std::fstream db_io_m;
    std::filesystem::path db_file_path_m;

    std::shared_ptr<spdlog::logger> logger_m;
};
