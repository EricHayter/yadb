#pragma once

#include "common/type_definitions.h"
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <span>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <string_view>
#include <unordered_set>

/**
 * @brief Disk manager class to handle the database file
 *
 * This class provides the functionality for the disk manager which handles
 * all associated I/O operations required loading, and writing to disk for the
 * page buffer manager. This class also provides additional functionality
 * for allocating new pages inside of the database file.
 */
class DiskManager {
public:
    DiskManager(const std::filesystem::path& db_directory);
    DiskManager(const std::filesystem::path& db_directory, std::size_t page_capacity);
    ~DiskManager();

    /**
     * @brief allocated a new page in the database file for writing to
     *
     * This function will either a) use a "free" page already existing in the
     * database file or b) increase the capacity of the database file
     *
     * @returns the page id of the newly allocated page
     */
    page_id_t AllocatePage();

    /**
     * @brief Write page data to disk
     *
     * @param page_id the page to write the data to
     * @page_data the data to write to the page
     *
     * @returns true if the write was successful otherwise returns false
     */
    bool WritePage(page_id_t page_id, PageView page_data);

    /**
     * @brief Read data from disk
     *
     * @param page_id the page to read the data from
     * @param page_data a span to write the page data to
     *
     * @returns true if the read was successful otherwise returns false
     */
    bool ReadPage(page_id_t page_id, MutPageView page_data);

    /**
     * @brief Deletes a page from the database file
     *
     * NOTE: this only LOGICALLY deletes the page from the database file. The
     * data is in fact still there and there is no shrinkage of the database
     * file itself. The page may then reused when allocating new pages.
     *
     * @param the page to delete from the database file
     */
    void DeletePage(page_id_t page_id);

private:
    static constexpr std::string_view DB_FILE_NAME = "data.db";
    static constexpr std::string_view LOG_FILE_NAME = "disk_manager.log";
    static constexpr std::string_view LOGGER_NAME = "disk_manager_logger";

    std::size_t GetOffset(page_id_t page_id);
    std::size_t GetDatabaseFileSize();

    std::size_t page_capacity_m;

    /// list of pages that are considered free
    std::unordered_set<page_id_t> free_pages_m;

    std::fstream db_io_m;
    std::filesystem::path db_file_path_m;
    std::filesystem::path db_directory_m;
    std::shared_ptr<spdlog::logger> logger_m;
};
