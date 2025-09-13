#include "buffer/page_buffer_manager.h"
#include "buffer/frame_header.h"
#include "config/config.h"
#include <cassert>
#include <future>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

PageBufferManager::PageBufferManager(const DatabaseConfig& config, std::size_t num_frames)
    : disk_scheduler_m(config)
    , logger_m(config.page_buffer_manager_logger)
    , replacer_m()
    , buffer_m((char*)malloc(num_frames * PAGE_SIZE))
{
    assert(buffer_m != nullptr);
    for (frame_id_t id = 0; id < num_frames; id++) {
        MutPageView data_view(buffer_m + id * PAGE_SIZE, PAGE_SIZE);
        frames_m.push_back(std::make_unique<FrameHeader>(id, data_view));
        replacer_m.RegisterFrame(id);
    }
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
    page_id_t page_id = page_future.get();
    auto page = WaitWritePage(page_id);
    assert(page.has_value());
    page->InitPage();
    return page_id;
}

std::optional<Page> PageBufferManager::TryReadPage(page_id_t page_id)
{
    // acquire a lock so that the page isn't flushed from the frame as
    // we are creating the page guard.
    std::lock_guard<std::mutex> lk(mut_m);
    if (LoadPage(page_id) != LoadPageStatus::Success) {
        return std::nullopt;
    }
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    if (not frame->mut.try_lock_shared()) {
        return std::nullopt;
    }
    AddAccessor(frame->id, false);
    std::shared_lock<std::shared_mutex> frame_lk(frame->mut, std::adopt_lock);
    return std::make_optional<Page>(this, page_id, frame->GetMutData(), std::move(frame_lk));
}

std::optional<Page> PageBufferManager::WaitReadPage(page_id_t page_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    available_frame_m.wait(lk, [this, page_id]() {
        return page_map_m.contains(page_id) || replacer_m.GetEvictableCount() > 0;
    });

    if (not page_map_m.contains(page_id)) {
        if (LoadPage(page_id) != LoadPageStatus::Success) {
            return std::nullopt;
        }
    }
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    AddAccessor(frame->id, false);
    lk.unlock();

    frame->mut.lock_shared();
    std::shared_lock<std::shared_mutex> frame_lk(frame->mut, std::adopt_lock);
    return std::make_optional<Page>(this, page_id, frame->GetMutData(), std::move(frame_lk));
}

std::optional<PageMut> PageBufferManager::TryWritePage(page_id_t page_id)
{
    // acquire a lock so that the page isn't flushed from the frame as
    // we are creating the page guard.
    std::lock_guard<std::mutex> lk(mut_m);
    if (LoadPage(page_id) != LoadPageStatus::Success) {
        return std::nullopt;
    }
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    if (not frame->mut.try_lock()) {
        return std::nullopt;
    }
    AddAccessor(frame->id, true);
    std::unique_lock<std::shared_mutex> frame_lk(frame->mut, std::adopt_lock);
    return std::make_optional<PageMut>(this, page_id, frame->GetMutData(), std::move(frame_lk));
}

std::optional<PageMut> PageBufferManager::WaitWritePage(page_id_t page_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    available_frame_m.wait(lk, [this, page_id]() {
        return page_map_m.contains(page_id) || replacer_m.GetEvictableCount() > 0;
    });

    if (not page_map_m.contains(page_id)) {
        if (LoadPage(page_id) != LoadPageStatus::Success) {
            return std::nullopt;
        }
    }

    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    AddAccessor(frame->id, true);
    lk.unlock();

    frame->mut.lock();
    std::unique_lock<std::shared_mutex> frame_lk(frame->mut, std::adopt_lock);
    return std::make_optional<PageMut>(this, page_id, frame->GetMutData(), std::move(frame_lk));
}

PageBufferManager::LoadPageStatus PageBufferManager::LoadPage(page_id_t page_id)
{
    // it's already loaded no more IO to handle
    if (page_map_m.contains(page_id)) {
        return LoadPageStatus::Success;
    }

    // Our page is not already in the page buffer
    // We must evict a frame to find a spot for our page
    std::optional<frame_id_t> frame_id_opt = replacer_m.EvictFrame();
    if (not frame_id_opt.has_value()) {
        logger_m->info("Couldn't find a frame to evict for page {}", page_id);
        return LoadPageStatus::NoFreeFrameError;
    }
    FrameHeader* frame = frames_m[*frame_id_opt].get();

    // if the current frame contains page data that was updated
    if (frame->is_dirty && FlushPage(frame->page_id) != FlushPageStatus::Success) {
        logger_m->warn("Failed to load page {} due to flush failure", page_id);
        return LoadPageStatus::IOError;
    }

    // unmap the old page from the page-to-frame map.
    page_map_m.erase(frame->page_id);

    // read the desired page data from disk to the frame
    std::promise<bool> read_status_promise;
    std::future<bool> read_status_future = read_status_promise.get_future();
    disk_scheduler_m.ReadPage(page_id, frame->GetMutData(), std::move(read_status_promise));
    if (not read_status_future.get()) {
        logger_m->warn("Failed to load page {} due to read failure", page_id);
        return LoadPageStatus::IOError;
    }

    // if the frame is now loaded you don't necessarily need to evict another
    // page if a page guard requires data to this frame now.
    available_frame_m.notify_all();

    // update frame information
    page_map_m[page_id] = frame->id;
    frame->page_id = page_id;
    frame->is_dirty = false;
    frame->pin_count = 0;
    return LoadPageStatus::Success;
}

// Make this function "Frame-centric"
PageBufferManager::FlushPageStatus PageBufferManager::FlushPage(page_id_t page_id)
{
    std::promise<bool> write_status_promise;
    std::future<bool> write_status_future = write_status_promise.get_future();
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    disk_scheduler_m.WritePage(page_id, frame->GetData(), std::move(write_status_promise));
    if (not write_status_future.get()) {
        logger_m->warn("Failed to flush page {}", page_id);
        return FlushPageStatus::IOError;
    }
    return FlushPageStatus::Success;
}

void PageBufferManager::AddAccessor(page_id_t page_id, bool is_writer)
{
    replacer_m.RecordAccess(page_id);
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    frame->pin_count++;
    frame->is_dirty = frame->is_dirty || is_writer;
}

void PageBufferManager::RemoveAccessor(page_id_t page_id)
{
    std::lock_guard<std::mutex> lk(mut_m);
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    frame->pin_count--;

    // This frame is now ready for eviction if needed
    if (frame->pin_count == 0) {
        replacer_m.SetEvictable(frame->id, true);
        available_frame_m.notify_one();
    }
}
