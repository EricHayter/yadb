#include "storage/on_disk/buffer_manager/page_buffer_manager.h"
#include "config/config.h"
#include "spdlog/fmt/bundled/base.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/logger.h"
#include "storage/on_disk/buffer_manager/frame.h"
#include "storage/on_disk/buffer_manager/lru_k_replacer.h"
#include "storage/on_disk/buffer_manager/page.h"
#include "storage/on_disk/disk/disk_manager.h"
#include <atomic>
#include <cassert>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stdlib.h>
#include <string>

PageBufferManager::PageBufferManager()
    : PageBufferManager(128)
{
}

PageBufferManager::PageBufferManager(std::size_t num_frames)
    : PageBufferManager(DatabaseConfig::CreateNull(), num_frames)
{
}

PageBufferManager::PageBufferManager(const DatabaseConfig& config, std::size_t num_frames)
    : disk_manager_m(config)
    , logger_m(config.page_buffer_manager_logger)
    , replacer_m()
    , buffer_m((char*)malloc(num_frames * PAGE_SIZE))
{
    assert(buffer_m != nullptr);
    for (frame_id_t id = 0; id < num_frames; id++) {
        MutFullPage data_view(reinterpret_cast<std::byte*>(buffer_m + id * PAGE_SIZE), PAGE_SIZE);
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

page_id_t PageBufferManager::AllocatePage(file_id_t file_id)
{
    return disk_manager_m.AllocatePage(file_id);
}

Page PageBufferManager::GetPage(const file_page_id_t& fp_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    available_frame_m.wait(lk, [this, fp_id]() {
        return page_map_m.contains(fp_id) || replacer_m.GetEvictableCount() > 0;
    });

    if (!page_map_m.contains(fp_id)) {
        if (LoadPage(fp_id) != LoadPageStatus::Success) {
            throw std::runtime_error("Failed to load page");
        }
    }

    return PinAndReturnPage(fp_id);
}

std::optional<Page> PageBufferManager::GetPageIfFrameAvailable(const file_page_id_t& fp_id)
{
    std::unique_lock<std::mutex> lk(mut_m);

    /* If there is no way to "immediately" (i.e. no free slots we can replace
     * and not in cache) get the page without waiting then return early */
    if (!page_map_m.contains(fp_id) && replacer_m.GetEvictableCount() == 0)
        return {};

    if (!page_map_m.contains(fp_id)) {
        if (LoadPage(fp_id) != LoadPageStatus::Success) {
            return {};
        }
    }

    return PinAndReturnPage(fp_id);
}

PageBufferManager::LoadPageStatus PageBufferManager::LoadPage(const file_page_id_t& fp_id)
{
    // it's already loaded no more IO to handle
    if (page_map_m.contains(fp_id)) {
        return LoadPageStatus::Success;
    }

    // Our page is not already in the page buffer
    // We must evict a frame to find a spot for our page
    std::optional<frame_id_t> frame_id_opt = replacer_m.EvictFrame();
    if (!frame_id_opt.has_value()) {
        logger_m->info("Couldn't find a frame to evict for page {}", fp_id.page_id);
        return LoadPageStatus::NoFreeFrameError;
    }
    Frame* frame = frames_m[*frame_id_opt].get();

    // if the current frame contains page data that was updated
    if (frame->is_dirty && FlushPage(frame->fp_id) != FlushPageStatus::Success) {
        logger_m->warn("Failed to load page {} due to flush failure", fp_id.page_id);
        return LoadPageStatus::IOError;
    }

    // unmap the old page from the page-to-frame map.
    page_map_m.erase(frame->fp_id);

    // read the desired page data from disk to the frame
    if (!disk_manager_m.ReadPage(fp_id, frame->data)) {
        logger_m->warn("Failed to load page {} due to read failure", fp_id.page_id);
        return LoadPageStatus::IOError;
    }

    // if the frame is now loaded you don't necessarily need to evict another
    // page if a page guard requires data to this frame now.
    available_frame_m.notify_all();

    // update frame information
    page_map_m[fp_id] = frame->id;
    frame->fp_id = fp_id;
    frame->is_dirty = false;
    frame->pin_count = 0;
    return LoadPageStatus::Success;
}

PageBufferManager::FlushPageStatus PageBufferManager::FlushPage(const file_page_id_t& fp_id)
{
    Frame* frame = GetFrameForPage(fp_id);
    if (!disk_manager_m.WritePage(fp_id, frame->data)) {
        logger_m->warn("Failed to flush page {}", fp_id.page_id);
        return FlushPageStatus::IOError;
    }
    return FlushPageStatus::Success;
}

void PageBufferManager::RemoveAccessor(const file_page_id_t& fp_id)
{
    std::lock_guard<std::mutex> lk(mut_m);
    Frame* frame = GetFrameForPage(fp_id);
    auto prev = frame->pin_count.fetch_sub(1, std::memory_order_acq_rel);

    assert(prev > 0 && "RemoveAccessor called when pin_count == 0");

    // This frame is now ready for eviction if needed
    if (prev == 1) {
        replacer_m.SetEvictable(frame->id, true);
        available_frame_m.notify_one();
    }
}

Frame* PageBufferManager::GetFrameForPage(const file_page_id_t& fp_id) const
{
    auto it = page_map_m.find(fp_id);
    if (it == page_map_m.end()) {
        throw std::runtime_error("Failed to get frame for page " + std::to_string(fp_id.page_id) + " - page not in buffer pool");
    }
    return frames_m[it->second].get();
}

Page PageBufferManager::PinAndReturnPage(const file_page_id_t& fp_id)
{
    Frame* frame = GetFrameForPage(fp_id);
    frame->pin_count.fetch_add(1, std::memory_order_acq_rel);
    replacer_m.RecordAccess(frame->id);
    replacer_m.SetEvictable(frame->id, false);
    return Page(this, frame);
}
