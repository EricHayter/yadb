#pragma once

#include "buffer/frame_header.h"
#include "buffer/lru_k_replacer.h"
#include "buffer/page_guard.h"
#include "common/type_definitions.h"
#include "storage/disk/disk_scheduler.h"
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <condition_variable>
#include <filesystem>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

class ReadPageGuard;
class WritePageGuard;

/**
 * @brief Page Buffer Manager
 *
 * This class provides the management of a large buffer meant to temporarily
 * store page data for fast reads and writes (whwen compared to disk I/O).
 * The page buffer manager consists of many "frames" that are small segments
 * of the buffer that act as slots to hold page data.
 *
 * To facilitate the use of the page buffer manager some additional classes
 * are used:
 * 1. LRU-K replacement policy: since the page buffer itself has limited
 *    storage once the cache itself is full (i.e. all frames are filled) the
 *    replacement policy determines which page data can be flushed to disk
 *    to free up a frame for new page data.
 * 2. Page Guards: page guards interact with the page buffer manager by
 *    creating a handle to a view of the data in the buffer for a specific
 *    page. These classes use RAII to keep track of the pin count of each
 *    frame to allow for concurrent access.
 * 3. Frame Headers: keeps track of important metadata about frames including
 *    the amount of current accessors (pin count) and whether or not the page
 *    has been written to (dirtiness) which may affect flushing page data.
 *
 *
 * Additional information for these classes can be found in their respective
 * header files.
 *
 * All requirements for disk I/O for reading and writing data to and from disk
 * is handled by the disk scheduler.
 */
class PageBufferManager {
    // TODO refactor this to make the deconstructors friend classes
    friend ReadPageGuard;
    friend WritePageGuard;

public:
    PageBufferManager(const std::filesystem::path& db_directory, std::size_t num_frames);
    ~PageBufferManager();

    /**
     * @brief Creates a new page
     *
     * @returns the id of the newly created page
     */
    page_id_t NewPage();

    /**
     * @brief Try to create a reading page guard
     *
     * Attempts to create a read guard for reading page data from the page
     * buffer. The function may fail to acquire a read page guard and return
     * a std::nullopt in two cases:
     * 1. A read failure from the disk scheduler doesn't allow the page data
     *    to be read into the buffer.
     * 2. A writer has already gained access to the page so I reading handle
     *    cannot be created to allow for mutually exclusive single writers and
     *    multiple readers.
     *
     * @param page_id the page to create a read guard for
     *
     * @returns a reading page guard if possible
     */
    std::optional<ReadPageGuard> TryReadPage(page_id_t page_id);

    /**
     * @brief Wait to acquire a read page guard
     *
     * This function will wait until it is possible to create a read page
     * guard. The function will block if a writing page guard already exists
     * for the page and unblock once the write page guard is deconstructed
     *
     * @param page_id the page to create a read guard for
     *
     * @returns a reading page guard
     */
    // TODO this might need to be an optional since reads could fail
    ReadPageGuard WaitReadPage(page_id_t page_id);

    std::optional<WritePageGuard> TryWritePage(page_id_t page_id);
    WritePageGuard WaitWritePage(page_id_t page_id);

private:
    /**
     * @brief Attempts to load a page into the page buffer
     *
     * @param page_id the page id of the page to load
     *
     * @returns true if the page was successfully loaded and false otherwise
     */
    bool LoadPage(page_id_t page_id);

    /**
     * @brief Flushes a page from the page buffer
     *
     * Writes the page contents of a given frame to disk
     *
     * @param page_id the page to flush to disk
     *
     * @returns true if the flush was successful and false otherwise
     */
    bool FlushPage(page_id_t page_id);

    /**
     * @brief Notify the page buffer manager that a frame has been accessed
     *
     * @param frame_id the frame that was accessed
     * @param is_writer was the access for writes
     */
    void AddAccessor(frame_id_t frame_id, bool is_writer);

    /**
     * @brief Notify the page buffer manager that an accessor has been dropped
     * from a frame
     *
     * @param frame_id the frame that was being accessed
     */
    void RemoveAccessor(frame_id_t frame_id);

private:
    static constexpr std::string_view PAGE_BUFFER_MANAGER_LOG_FILENAME { "page_buffer_manager.log" };
    std::shared_ptr<spdlog::logger> logger_m;

    LRUKReplacer replacer_m;
    DiskScheduler disk_scheduler_m;

    char* buffer_m; /// data buffer

    std::unordered_map<page_id_t, frame_id_t> page_map_m;
    std::vector<std::unique_ptr<FrameHeader>> frames_m;

    std::mutex mut_m;
    std::condition_variable available_frame_m;
};
