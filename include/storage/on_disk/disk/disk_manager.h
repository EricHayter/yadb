#pragma once

#include "core/error.h"
#include "storage/on_disk/types.h"
#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

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
    DiskManager() = default;
    ~DiskManager();

    /*
     * Creates a new file and returns the corresponding file_id_t.
     */
    std::expected<file_id_t, yadb::Error::Ptr> CreateFile();

    /*
     * Creates a new file with the specified file_id.
     */
    std::optional<yadb::Error::Ptr> CreateFile(file_id_t file_id);

    /*
     * Allocates a new page in the database file for writing to.
     *
     * This function will either a) use a "free" page already existing in the
     * database file or b) increase the capacity of the database file.
     */
    std::expected<page_id_t, yadb::Error::Ptr> AllocatePage(file_id_t file_id);

    /*
     * Write page data to disk.
     */
    std::optional<yadb::Error::Ptr> WritePage(const file_page_id_t& fp_id, FullPage page);

    /*
     * Read data from disk.
     */
    std::optional<yadb::Error::Ptr> ReadPage(const file_page_id_t& fp_id, MutFullPage page);

    /*
     * Deletes a page from the database file
     *
     * NOTE: this only LOGICALLY deletes the page from the database file. The
     * data is in fact still there and there is no shrinkage of the database
     * file itself. The page may then reused when allocating new pages.
     */
    std::optional<yadb::Error::Ptr> DeletePage(const file_page_id_t& fp_id);

    /*
     * Closes and removes the physical file for the given file_id.
     */
    std::optional<yadb::Error::Ptr> DeleteFile(file_id_t file_id);

private:
    struct DatabaseFile {
        using Ptr = std::shared_ptr<DatabaseFile>;

        mutable std::unique_ptr<std::mutex> mut;
        std::filesystem::path path;

        /* iostream to write to database file */
        std::fstream file_stream;

        /* list of pages that are considered free */
        std::unordered_set<page_id_t> free_pages;

        std::size_t PageCapacity() const
        {
            return std::filesystem::file_size(path) / PAGE_SIZE;
        }
    };

    std::unordered_map<file_id_t, DatabaseFile::Ptr> id_map_m;
    std::expected<DatabaseFile::Ptr, yadb::Error::Ptr> GetFileHandle(file_id_t file_id);

    static std::filesystem::path GetFilePath(file_id_t file_id);
    std::size_t GetOffset(page_id_t page_id) const;

    mutable std::mutex mut_m;
};
