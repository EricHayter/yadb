#include "buffer/page_buffer_manager.h"

#include <cassert>

#include <future>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "buffer/frame.h"
#include "config/config.h"
#include "page/page.h"

PageBufferManager::PageBufferManager()
    : PageBufferManager(128)
{
}

PageBufferManager::PageBufferManager(std::size_t num_frames)
    : PageBufferManager(DatabaseConfig::CreateNull(), num_frames)
{
}

PageBufferManager::PageBufferManager(const DatabaseConfig& config, std::size_t num_frames)
    : disk_scheduler_m(config)
    , logger_m(config.page_buffer_manager_logger)
    , replacer_m()
    , buffer_m((char*)malloc(num_frames * PAGE_SIZE))
{
    assert(buffer_m != nullptr);
    for (frame_id_t id = 0; id < num_frames; id++) {
        MutPageView data_view(buffer_m + id * PAGE_SIZE, PAGE_SIZE);
        frames_m.push_back(std::make_unique<Frame>(id, data_view));
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

page_id_t PageBufferManager::AllocatePage()
{
    /* request the disk scheduler to create a page */
    std::promise<page_id_t> page_promise;
    std::future<page_id_t> page_future = page_promise.get_future();
    disk_scheduler_m.AllocatePage(std::move(page_promise));
    page_id_t page_id = page_future.get();

    /* we're gonna create the page manually without even creating a handle
     * this bellow is a nightmare waiting to happen */
    std::lock_guard<std::mutex> lk(mut_m);
    if (LoadPage(page_id) != LoadPageStatus::Success) {
        // TODO THIS IS A MASSIVE HACK I NEED TO REMOVE THE DISK SCHEDULER
        return -1;
    }

    // TODO this isn't my favorite either tbh... It seems a bit hacky to
    // not include a pointer to the page buffer manager JUST to init the page
    // This almost certainly won't work when we start adding page types.
    // Since this function will likely need to be templated
    Frame* frame = GetFrameForPage(page_id);
    std::unique_lock<std::shared_mutex> frame_lk(frame->mut);
    MutPage page(nullptr, page_id, frame->data, std::move(frame_lk));
    page.InitPage();
    page.UpdateChecksum();

    return page_id;
}

std::optional<Page> PageBufferManager::TryReadPage(page_id_t page_id)
{
    // acquire a lock so that the page isn't flushed from the frame as
    // we are creating the page guard.
    std::unique_lock<std::mutex> lk(mut_m);
    if (LoadPage(page_id) != LoadPageStatus::Success) {
        return std::nullopt;
    }
    Frame* frame = GetFrameForPage(page_id);
    if (!frame->mut.try_lock_shared()) {
        return std::nullopt;
    }
    std::shared_lock<std::shared_mutex> frame_lk(frame->mut, std::adopt_lock);

    Page page(this, page_id, frame->GetMutData(), std::move(frame_lk));

    /* need to unlock here early in case the validation for checksum fails and
     * the page is deconstructed calling remove accessor (which requires locK)
     * */
    lk.unlock();
    if (!page.ValidChecksum()) {
        throw ChecksumValidationException(page_id, "Checksum is invalid! Check database file for corruption.");
    }

    return std::make_optional<Page>(std::move(page));
}

Page PageBufferManager::ReadPage(page_id_t page_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    available_frame_m.wait(lk, [this, page_id]() {
        return page_map_m.contains(page_id) || replacer_m.GetEvictableCount() > 0;
    });

    if (!page_map_m.contains(page_id)) {
        if (LoadPage(page_id) != LoadPageStatus::Success) {
            throw std::runtime_error("Failed to load page");
        }
    }
    Frame* frame = GetFrameForPage(page_id);
    std::shared_lock<std::shared_mutex> frame_lk(frame->mut);

    Page page(this, page_id, frame->GetMutData(), std::move(frame_lk));

    /* need to unlock here early in case the validation for checksum fails and
     * the page is deconstructed calling remove accessor (which requires locK)
     * */
    lk.unlock();
    if (!page.ValidChecksum()) {
        throw ChecksumValidationException(page_id, "Checksum is invalid! Check database file for corruption.");
    }

    return page;
}

std::optional<MutPage> PageBufferManager::TryWritePage(page_id_t page_id)
{
    // acquire a lock so that the page isn't flushed from the frame as
    // we are creating the page guard.
    std::unique_lock<std::mutex> lk(mut_m);
    if (LoadPage(page_id) != LoadPageStatus::Success) {
        return std::nullopt;
    }
    Frame* frame = GetFrameForPage(page_id);
    if (!frame->mut.try_lock()) {
        return std::nullopt;
    }
    std::unique_lock<std::shared_mutex> frame_lk(frame->mut, std::adopt_lock);

    MutPage page(this, page_id, frame->GetMutData(), std::move(frame_lk));
    /* need to unlock here early in case the validation for checksum fails and
     * the page is deconstructed calling remove accessor (which requires locK)
     * */
    lk.unlock();
    if (!page.ValidChecksum()) {
        throw ChecksumValidationException(page_id, "Checksum is invalid! Check database file for corruption.");
    }

    return std::make_optional<MutPage>(std::move(page));
}

MutPage PageBufferManager::WritePage(page_id_t page_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    available_frame_m.wait(lk, [this, page_id]() {
        return page_map_m.contains(page_id) || replacer_m.GetEvictableCount() > 0;
    });

    if (!page_map_m.contains(page_id)) {
        if (LoadPage(page_id) != LoadPageStatus::Success) {
            throw std::runtime_error("Failed to load page");
        }
    }

    Frame* frame = GetFrameForPage(page_id);
    std::unique_lock<std::shared_mutex> frame_lk(frame->mut);

    MutPage page(this, page_id, frame->GetMutData(), std::move(frame_lk));
    /* need to unlock here early in case the validation for checksum fails and
     * the page is deconstructed calling remove accessor (which requires locK)
     * */
    lk.unlock();
    if (!page.ValidChecksum()) {
        throw ChecksumValidationException(page_id, "Checksum is invalid! Check database file for corruption.");
    }

    return page;
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
    if (!frame_id_opt.has_value()) {
        logger_m->info("Couldn't find a frame to evict for page {}", page_id);
        return LoadPageStatus::NoFreeFrameError;
    }
    Frame* frame = frames_m[*frame_id_opt].get();

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
    if (!read_status_future.get()) {
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

PageBufferManager::FlushPageStatus PageBufferManager::FlushPage(page_id_t page_id)
{
    std::promise<bool> write_status_promise;
    std::future<bool> write_status_future = write_status_promise.get_future();
    Frame* frame = GetFrameForPage(page_id);
    disk_scheduler_m.WritePage(page_id, frame->GetData(), std::move(write_status_promise));
    if (!write_status_future.get()) {
        logger_m->warn("Failed to flush page {}", page_id);
        return FlushPageStatus::IOError;
    }
    return FlushPageStatus::Success;
}

void PageBufferManager::AddAccessor(page_id_t page_id, bool is_writer)
{
    /* Since this function is always called inside of the constructor of pages
     * and since pages are always created from the scope of WritePage, ReadPage
     * etc... We do not lock here since this function should always be called
     * from a locked context. */
    Frame* frame = GetFrameForPage(page_id);
    replacer_m.RecordAccess(frame->id);
    frame->pin_count++;
    frame->is_dirty = frame->is_dirty || is_writer;
}

void PageBufferManager::RemoveAccessor(page_id_t page_id)
{
    std::lock_guard<std::mutex> lk(mut_m);
    Frame* frame = GetFrameForPage(page_id);
    frame->pin_count--;

    // This frame is now ready for eviction if needed
    if (frame->pin_count == 0) {
        replacer_m.SetEvictable(frame->id, true);
        available_frame_m.notify_one();
    }
}

Frame* PageBufferManager::GetFrameForPage(page_id_t page_id) const
{
    auto it = page_map_m.find(page_id);
    if (it == page_map_m.end()) {
        throw std::runtime_error("Failed to get frame for page " + std::to_string(page_id) + " - page not in buffer pool");
    }
    return frames_m[it->second].get();
}
