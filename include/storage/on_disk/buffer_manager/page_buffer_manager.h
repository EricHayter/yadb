#pragma once

#include "core/error.h"
#include "storage/on_disk/buffer_manager/frame.h"
#include "storage/on_disk/buffer_manager/lru_k_replacer.h"
#include "storage/on_disk/buffer_manager/page.h"
#include "storage/on_disk/disk/disk_manager.h"
#include "storage/on_disk/types.h"
#include <condition_variable>
#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

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
 * All requirements for disk I/O for reading and writing data to and from disk
 * is handled by the disk manager.
 */
class PageBufferManager {
    friend Page;

public:
    PageBufferManager();
    PageBufferManager(std::size_t num_frames);
    ~PageBufferManager();

    std::expected<file_id_t, yadb::Error::Ptr> CreateFile();
    std::optional<yadb::Error::Ptr> CreateFile(file_id_t file_id);

    bool FileExists(file_id_t file_id) const;

    /**
     * Evicts all cached pages belonging to file_id and deletes the physical
     * file. Blocks until all pinned pages are released, up to a 5-second
     * timeout.
     */
    std::optional<yadb::Error::Ptr> DeleteFile(file_id_t file_id);

    std::expected<page_id_t, yadb::Error::Ptr> AllocatePage(file_id_t file_id);

    std::expected<Page, yadb::Error::Ptr> GetPage(const file_page_id_t& fp_id);

    /**
     * Retrieves a page if a buffer frame is immediately available.
     * Returns success with std::nullopt if no frame is available without waiting.
     * Returns an error if a frame was available but an IO failure occurred.
     */
    std::expected<std::optional<Page>, yadb::Error::Ptr> GetPageIfFrameAvailable(const file_page_id_t& fp_id);

private:
    std::optional<yadb::Error::Ptr> LoadPage(const file_page_id_t& fp_id);
    std::optional<yadb::Error::Ptr> FlushPage(const file_page_id_t& fp_id);

    bool FileHasPinnedPages(file_id_t file_id) const;

    Frame* GetFrameForPage(const file_page_id_t& fp_id) const;

    void RemoveAccessor(const file_page_id_t& fp_id);

    Page PinAndReturnPage(const file_page_id_t& fp_id);

private:
    LRUKReplacer replacer_m;
    DiskManager disk_manager_m;

    char* buffer_m;

    std::unordered_map<file_page_id_t, frame_id_t> page_map_m;
    std::vector<std::unique_ptr<Frame>> frames_m;

    std::mutex mut_m;
    std::condition_variable available_frame_m;
};
