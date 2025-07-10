#pragma once
#include "buffer/frame_header.h"
#include "buffer/page_buffer_manager.h"
#include "common/type_definitions.h"
#include <memory>
#include <mutex>
#include <shared_mutex>

/* ============================================================================
 * Page Guards
 * ============================================================================
 * To allow for concurrent access to frames in the page buffer manager, page
 * guards allow for a thread safe view into the buffers. This is done using
 * a shared_lock allowing multiple readers at once to a frame OR a single
 * writer. Both page guards should be responsible for automatically interacting
 * with the page buffer manager to indicate when access has been gained and
 * released to a page (needed for the replacement policy).
 */

class PageBufferManager;

/**
 * @brief Thread safe read view into page data
 *
 * This class provides a thread-safe mechanism for reading data from a shared
 * page buffer from inside of the page buffer manager. This class allows for
 * multiple concurrent readers to a page buffer, however page read and write
 * guards should be mutually exclusive on the same page otherwise race
 * conditions may be created.
 */
class ReadPageGuard {
public:
    /**
     * @brief Constructs a read page guard
     *
     * @param page_buffer_manager a pointer to the
     * @param frame_header pointer to the frame information for the frame that
     * contains the page
     * @param lk a shared lock acquired on the data
     */
    // TODO could this be a page instead? it makes it seem a lot cleaner
    ReadPageGuard(PageBufferManager* page_buffer_manager, FrameHeader* frame_header, std::shared_lock<std::shared_mutex>&& lk);
    ~ReadPageGuard();
    ReadPageGuard(ReadPageGuard&& other);
    ReadPageGuard& operator=(ReadPageGuard&& other);

    /**
     * @brief Get a readable span to a page buffer
     *
     * @returns a read-only span to a page buffer (frame)
     */
    PageView GetData();

private:
    PageBufferManager* page_buffer_manager_m;
    FrameHeader* frame_header_m;
    std::shared_lock<std::shared_mutex> frame_lk_m;
};

/**
 * @brief Thread safe write view into page data
 *
 * This class provides a threadsafe mechanism for writing to a page buffer
 * inside of the page buffer manager. This class ensures that only a single
 * accesor to the given page at one given time.
 */
class WritePageGuard {
public:
    /**
     * @brief Constructs a write page guard
     *
     * @param page_buffer_manager a pointer to the
     * @param frame_header pointer to the frame information for the frame that
     * contains the page
     * @param lk an exclusive lock to the data in the page buffer
     */
    // TODO could this be a page instead? it makes it seem a lot cleaner
    WritePageGuard(PageBufferManager* page_buffer_manager, FrameHeader* frame_header, std::unique_lock<std::shared_mutex>&& lk);
    ~WritePageGuard();
    WritePageGuard(WritePageGuard&& other);
    WritePageGuard& operator=(WritePageGuard&& other);

    /**
     * @brief Get a writable span to a page buffer
     *
     * @returns a writable span to a page buffer (frame)
     */
    MutPageView GetData();

private:
    PageBufferManager* page_buffer_manager_m;
    FrameHeader* frame_header_m;
    std::unique_lock<std::shared_mutex> frame_lk_m;
};
