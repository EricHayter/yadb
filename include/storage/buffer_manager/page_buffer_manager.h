#pragma once

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "common/definitions.h"
#include "storage/buffer_manager/frame.h"
#include "storage/buffer_manager/lru_k_replacer.h"
#include "storage/disk/disk_scheduler.h"
#include "storage/buffer_manager/page.h"
namespace spdlog { class logger; }
struct DatabaseConfig;

/**
 * Page Buffer Manager
 *
 * This class provides the management of a large buffer meant to temporarily
 * store page data for fast reads and writes (when compared to disk I/O).
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

public:
    PageBufferManager();
    PageBufferManager(std::size_t num_frames);
    PageBufferManager(const DatabaseConfig& config, std::size_t num_frames);
    ~PageBufferManager();

    /**
     * Creates a new page by signalling to the disk manager
     */
    page_id_t AllocatePage();

    Page GetPage(page_id_t page_id);

    /* Retrieves a page if a buffer frame is immediately available.
     * Returns std::nullopt if the page is not cached and no frames can be evicted.
     * Will block on disk I/O if the page needs to be loaded from disk.
     * Will block briefly to acquire the buffer pool lock. */
    std::optional<Page> GetPageIfFrameAvailable(page_id_t page_id);

private:
    enum class LoadPageStatus {
        Success,
        IOError,
        NoFreeFrameError,
    };

    LoadPageStatus LoadPage(page_id_t page_id);

    enum class FlushPageStatus {
        Success,
        IOError,
    };

    FlushPageStatus FlushPage(page_id_t page_id);

    /**
     * Notify the page buffer manager that an accessor has been dropped
     * from a frame
     */
    void RemoveAccessor(page_id_t page_id);

    /*
     * returns a pointer to the frame header for the frame containing the
     * page with id page_id
     *
     * NOTE: this function WILL throw a runtime exception if the page is
     * not located inside of a frame. This function should be used to prevent
     * accidentally creating entries in the page map.
     */
    Frame* GetFrameForPage(page_id_t page_id) const;

    /*
     * Pins a page that is already loaded in the buffer pool and returns
     * a Page handle to it. Increments the pin count, records access for
     * the LRU-K replacer, and marks the frame as non-evictable.
     */
    Page PinAndReturnPage(page_id_t page_id);

private:
    std::shared_ptr<spdlog::logger> logger_m;

    LRUKReplacer replacer_m;

    DiskScheduler disk_scheduler_m;

    /* buffer for the entire page buffer pool */
    char* buffer_m;

    /* the map of page's to their corresponding frame containing their data */
    std::unordered_map<page_id_t, frame_id_t> page_map_m;

    /* all frame metadata */
    std::vector<std::unique_ptr<Frame>> frames_m;

    std::mutex mut_m;
    std::condition_variable available_frame_m;
};
