#include "buffer/buffer_pool_manager.h"
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

BufferPoolManager::BufferPoolManager(std::size_t num_frames, const std::filesystem::path& db_file)
    : disk_scheduler_m(db_file)
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

BufferPoolManager::~BufferPoolManager()
{
    for (auto [page_id, _] : page_map_m) {
        FlushPage(page_id);
    }
    free(buffer_m);
}

page_id_t BufferPoolManager::NewPage()
{
    std::promise<page_id_t> page_promise;
    std::future<page_id_t> page_future = page_promise.get_future();
    disk_scheduler_m.AllocatePage(std::move(page_promise));
    return page_future.get();
}

std::optional<ReadPageGuard> BufferPoolManager::TryReadPage(page_id_t page_id)
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

ReadPageGuard BufferPoolManager::WaitReadPage(page_id_t page_id)
{
    // acquire a lock so that the page isn't flushed from the frame as
    // we are creating the page guard.
    std::unique_lock<std::mutex> lk(mut_m);
    available_frame_m.wait(lk, [this, page_id]() { return LoadPage(page_id); });
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    std::shared_lock<std::shared_mutex> frame_lk(frame->mut);
    return ReadPageGuard(this, frame, std::move(frame_lk));
}

std::optional<WritePageGuard> BufferPoolManager::TryWritePage(page_id_t page_id)
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

WritePageGuard BufferPoolManager::WaitWritePage(page_id_t page_id)
{
    // acquire a lock so that the page isn't flushed from the frame as
    // we are creating the page guard.
    std::unique_lock<std::mutex> lk(mut_m);
    available_frame_m.wait(lk, [this, page_id]() { return LoadPage(page_id); });
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    std::unique_lock<std::shared_mutex> frame_lk(frame->mut);
    return WritePageGuard(this, frame, std::move(frame_lk));
}

bool BufferPoolManager::LoadPage(page_id_t page_id)
{
    if (page_map_m.contains(page_id)) {
        return true;
    }

    // Evict a frame (if possible)
    std::optional<frame_id_t> frame_id_opt = replacer_m.EvictFrame();
    if (not frame_id_opt.has_value()) {
        return false;
    }
    FrameHeader* frame = frames_m[*frame_id_opt].get();
    if (frame->is_dirty) {
        FlushPage(frame->page_id);
        page_map_m.erase(frame->page_id);
    }

    // update frame information
    page_map_m[page_id] = frame->id;
    frame->page_id = page_id;
    frame->is_dirty = false;
    frame->pin_count = 0;
    return true;
}

void BufferPoolManager::FlushPage(page_id_t page_id)
{
    std::lock_guard<std::mutex> lk(mut_m);
    // TODO Could fit in some asserts here for sure
    std::promise<void> done_promise;
    std::future<void> done_future = done_promise.get_future();
    FrameHeader* frame = frames_m[page_map_m[page_id]].get();
    disk_scheduler_m.WritePage(page_id, frame->GetData(), std::move(done_promise));
    done_future.get();
}

void BufferPoolManager::AddAccessor(frame_id_t frame_id, bool is_writer)
{
    std::lock_guard<std::mutex> lk(mut_m);
    replacer_m.RecordAccess(frame_id);
    FrameHeader* frame = frames_m[frame_id].get();
    frame->pin_count++;
    frame->is_dirty = frame->is_dirty || is_writer;
}

void BufferPoolManager::RemoveAccessor(frame_id_t frame_id)
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
