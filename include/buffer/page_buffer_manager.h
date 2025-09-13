#pragma once

#include "buffer/frame_header.h"
#include "buffer/lru_k_replacer.h"
#include "config/config.h"
#include "storage/disk/disk_scheduler.h"
#include "storage/page/page.h"
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <condition_variable>
#include <memory>
#include <optional>
#include <unordered_map>

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
    friend Page;
    friend PageMut;

public:
    PageBufferManager();
    PageBufferManager(std::size_t num_frames);
    PageBufferManager(const DatabaseConfig& config, std::size_t num_frames);
    ~PageBufferManager();

    /**
     * @brief Creates a new page
     *
     * @returns the id of the newly created page
     */
    page_id_t NewPage();

    /**
     * @brief Try to give access to a read-only page
     *
     * Attempts to create a thread-safe handle to a page for reading page data
     * from the page buffer. The function may fail to acquire a page and return
     * a std::nullopt in two cases:
     * 1. A read failure from the disk scheduler doesn't allow the page data
     *    to be read into the buffer.
     * 2. Exclusive access to the page has already been given.
     *
     * @param page_id the page to create a read-only page handle for
     *
     * @returns a read-only page handle
     */
    std::optional<Page> TryReadPage(page_id_t page_id);

    /**
     * @brief Wait to acquire access to a read-only page
     *
     * @param page_id the page to create a read-only handle for
     *
     * @returns a read-only page handle
     */
    std::optional<Page> WaitReadPage(page_id_t page_id);

    /**
     * @brief Try to give access to a writable page
     *
     * Attempts to create a thread-safe handle to a page for writing page data
     * from the page buffer. The function may fail to acquire access and return
     * a std::nullopt in two cases:
     * 1. A read failure from the disk scheduler doesn't allow the page data
     *    to be read into the buffer.
     * 2. Exclusive access to the page cannot be acquired immediately.
     *
     * @param page_id the page to create a writable page handle for
     *
     * @returns a writable page handle
     */
    std::optional<PageMut> TryWritePage(page_id_t page_id);

    /**
     * @brief Wait to acquire access to a writable page
     *
     * @param page_id the page to create a writable handle for
     *
     * @returns a writeable page handle
     */
    std::optional<PageMut> WaitWritePage(page_id_t page_id);

private:
    /**
     * @brief Status codes for calls to LoadPage
     */
    enum class LoadPageStatus {
        Success,
        IOError,
        NoFreeFrameError,
    };

    /**
     * @brief Attempts to load a page into the page buffer
     *
     * @param page_id the page id of the page to load
     *
     * @returns true if the page was successfully loaded and false otherwise
     */
    LoadPageStatus LoadPage(page_id_t page_id);

    /**
     * @brief Status codes for calls to FlushPage
     */
    enum class FlushPageStatus {
        Success,
        IOError,
    };

    /**
     * @brief Flushes a page from the page buffer
     *
     * Writes the page contents of a given frame to disk
     *
     * @param page_id the page to flush to disk
     *
     * @returns true if the flush was successful and false otherwise
     */
    FlushPageStatus FlushPage(page_id_t page_id);

    /**
     * @brief Notify the page buffer manager that a page has been accessed
     * @param page_id the page that is being accessed
     * @param is_writer was the access for writes
     */
    void AddAccessor(page_id_t page_id, bool is_writer);

    /**
     * @brief Notify the page buffer manager that an accessor has been dropped
     * from a frame
     *
     * @param page_id the page that access has been given up
     */
    void RemoveAccessor(page_id_t page_id);

private:
    std::shared_ptr<spdlog::logger> logger_m;

    LRUKReplacer replacer_m;
    DiskScheduler disk_scheduler_m;

    char* buffer_m; /// data buffer

    std::unordered_map<page_id_t, frame_id_t> page_map_m;
    std::vector<std::unique_ptr<FrameHeader>> frames_m;

    std::mutex mut_m;
    std::condition_variable available_frame_m;
};
