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

class ReadPageGuard {
public:
    ReadPageGuard(PageBufferManager* page_buffer_manager, FrameHeader* frame_header, std::shared_lock<std::shared_mutex>&& lk);
    ~ReadPageGuard();
    ReadPageGuard(ReadPageGuard&& other);
    ReadPageGuard& operator=(ReadPageGuard&& other);

    PageView GetData();

private:
    PageBufferManager* page_buffer_manager_m;
    FrameHeader* frame_header_m;
    std::shared_lock<std::shared_mutex> lk_m;
};

class WritePageGuard {
public:
    WritePageGuard(PageBufferManager* page_buffer_manager, FrameHeader* frame_header, std::unique_lock<std::shared_mutex>&& lk);
    ~WritePageGuard();
    WritePageGuard(WritePageGuard&& other);
    WritePageGuard& operator=(WritePageGuard&& other);

    MutPageView GetData();

private:
    PageBufferManager* page_buffer_manager_m;
    FrameHeader* frame_header_m;
    std::unique_lock<std::shared_mutex> lk_m;
};
