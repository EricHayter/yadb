#include "buffer/page_buffer_manager.h"
#include "buffer/frame_header.h"
#include "buffer/page_guard.h"
#include "common/type_definitions.h"
#include <cassert>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

PageBufferManager::PageBufferManager(const std::filesystem::path& db_directory, std::size_t num_frames)
    : disk_scheduler_m(db_directory)
    , replacer_m()
    , buffer_m((char*)malloc(num_frames * PAGE_SIZE))
{
    assert(buffer_m != nullptr);
    for (frame_id_t id = 0; id < num_frames; id++) {
        MutPageView data_view(buffer_m + id * PAGE_SIZE, PAGE_SIZE);
        frames_m.push_back(std::make_unique<FrameHeader>(id, data_view));
        replacer_m.RegisterFrame(id);
    }

    logger_m = spdlog::basic_logger_mt("page buffer_manager logger", db_directory / PAGE_BUFFER_MANAGER_LOG_FILENAME);
    logger_m->info("Successfully initialized page buffer manager");
}

PageBufferManager::~PageBufferManager()
{
    for (auto [page_id, _] : page_map_m) {
        FlushPage(page_id);
    }
    free(buffer_m);
    logger_m->info("Closed page buffer manager");
}

page_id_t PageBufferManager::NewPage()
{
    std::promise<page_id_t> page_promise;
    std::future<page_id_t> page_future = page_promise.get_future();
    disk_scheduler_m.AllocatePage(std::move(page_promise));
    return page_future.get();
}

std::optional<ReadPageGuard> PageBufferManager::TryReadPage(page_id_t page_id)
{
    // acquire a lock so that the page isn't flushed from the frame as
    // we are creating the page guard.
    std::lock_guard<std::mutex> lk(mut_m);
    if (not LoadPage(page_id)) {
        return std::nullopt;
    }
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    if (not frame->mut.try_lock_shared()) {
        return std::nullopt;
    }
    std::shared_lock<std::shared_mutex> frame_lk(frame->mut, std::adopt_lock);
    return std::make_optional<ReadPageGuard>(this, frame, std::move(frame_lk));
}

ReadPageGuard PageBufferManager::WaitReadPage(page_id_t page_id)
{
    // This is not the prettiest solution, but it is effective. Without this
    // deadlocks become possible in cases where a page guard is constructed
    // for the same frame.
    //
    // For example 2 calls: WaitReadPage(0) and  WaitReadPage(0) could
    // cause an deadlock.
    //
    // The first call runs perfectly fine and acquires a lock on the FrameHeader
    // then second then acquires a lock to the page manager but then
    // waits to acquire the lock on the frame. But then the mutex for the
    // page buffer manager cannot be released as it is required in the
    // deconstructor of the page guard.
    while (true) {
        std::unique_lock<std::mutex> lk(mut_m);

        // wait until the page is able to be loaded into a frame
        available_frame_m.wait(lk, [this, page_id]() {
            return LoadPage(page_id);
        });

        FrameHeader* frame = frames_m[page_map_m[page_id]].get();

        // attempt to lock the mutex for the header
        if (frame->mut.try_lock_shared()) {
            // Success: now we have both locks, safe to proceed
            std::shared_lock<std::shared_mutex> frame_lk(frame->mut, std::adopt_lock);
            return ReadPageGuard(this, frame, std::move(frame_lk));
        }

        // Could not acquire frame lock: release `mut_m` and retry
        // This avoids deadlock and ensures consistency
        lk.unlock();

        std::this_thread::yield();
    }
}

std::optional<WritePageGuard> PageBufferManager::TryWritePage(page_id_t page_id)
{
    // acquire a lock so that the page isn't flushed from the frame as
    // we are creating the page guard.
    std::lock_guard<std::mutex> lk(mut_m);
    if (not LoadPage(page_id)) {
        return std::nullopt;
    }
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    if (not frame->mut.try_lock()) {
        return std::nullopt;
    }
    std::unique_lock<std::shared_mutex> frame_lk(frame->mut, std::adopt_lock);
    return std::make_optional<WritePageGuard>(this, frame, std::move(frame_lk));
}

WritePageGuard PageBufferManager::WaitWritePage(page_id_t page_id)
{
    // This is not the prettiest solution, but it is effective. Without this
    // deadlocks become possible in cases where a page guard is constructed
    // for the same frame.
    //
    // For example 2 calls: WaitWritePage(0) and  WaitWritePage(0) could
    // cause an deadlock.
    //
    // The first call runs perfectly fine and acquires a lock on the FrameHeader
    // then second then acquires a lock to the page buffer manager but then
    // waits to acquire the lock on the frame. But then the mutex for the
    // page buffer manager cannot be released as it is required in the
    // deconstructor of the page guard.
    while (true) {
        std::unique_lock<std::mutex> lk(mut_m);

        // wait until the page is able to be loaded into a frame
        available_frame_m.wait(lk, [this, page_id]() {
            return LoadPage(page_id);
        });

        FrameHeader* frame = frames_m[page_map_m[page_id]].get();

        // attempt to lock the mutex for the header
        if (frame->mut.try_lock()) {
            // Success: now we have both locks, safe to proceed
            std::unique_lock<std::shared_mutex> frame_lk(frame->mut, std::adopt_lock);
            return WritePageGuard(this, frame, std::move(frame_lk));
        }

        // Could not acquire frame lock: release `mut_m` and retry
        // This avoids deadlock and ensures consistency
        lk.unlock();

        std::this_thread::yield();
    }
}

bool PageBufferManager::LoadPage(page_id_t page_id)
{
    // it's already loaded no more IO to handle
    if (page_map_m.contains(page_id)) {
        return true;
    }

    // Our page is not already in the page buffer
    // We must evict a frame to find a spot for our page
    std::optional<frame_id_t> frame_id_opt = replacer_m.EvictFrame();
    if (not frame_id_opt.has_value()) {
        logger_m->info("Couldn't find a frame to evict for page {}", page_id);
        return false;
    }
    FrameHeader* frame = frames_m[*frame_id_opt].get();

    // if the current frame contains page data that was updated
    if (frame->is_dirty && not FlushPage(frame->page_id)) {
        logger_m->warn("Failed to load page {} due to flush failure", page_id);
        return false;
    }

    // unmap the old page from the page-to-frame map.
    page_map_m.erase(frame->page_id);

    // read the desired page data from disk to the frame
    std::promise<bool> read_status_promise;
    std::future<bool> read_status_future = read_status_promise.get_future();
    disk_scheduler_m.ReadPage(page_id, frame->GetMutData(), std::move(read_status_promise));
    if (not read_status_future.get()) {
        logger_m->warn("Failed to load page {} due to read failure", page_id);
        return false;
    }

    // update frame information
    page_map_m[page_id] = frame->id;
    frame->page_id = page_id;
    frame->is_dirty = false;
    frame->pin_count = 0;
    return true;
}

// TODO make this take a frame header
bool PageBufferManager::FlushPage(page_id_t page_id)
{
    std::promise<bool> write_status_promise;
    std::future<bool> write_status_future = write_status_promise.get_future();
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    disk_scheduler_m.WritePage(page_id, frame->GetData(), std::move(write_status_promise));
    if (not write_status_future.get()) {
        logger_m->warn("Failed to flush page {}", page_id);
        return false;
    }
    return true;
}

void PageBufferManager::AddAccessor(frame_id_t frame_id, bool is_writer)
{
    replacer_m.RecordAccess(frame_id);
    FrameHeader* frame = frames_m[frame_id].get();
    frame->pin_count++;
    frame->is_dirty = frame->is_dirty || is_writer;
}

void PageBufferManager::RemoveAccessor(frame_id_t frame_id)
{
    std::lock_guard<std::mutex> lk(mut_m);
    FrameHeader* frame = frames_m[frame_id].get();
    frame->pin_count--;

    // This frame is now ready for eviction if needed
    if (frame->pin_count == 0) {
        replacer_m.SetEvictable(frame_id, true);
        available_frame_m.notify_one();
    }
}
