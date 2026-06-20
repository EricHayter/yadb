#include "storage/on_disk/buffer_manager/page_buffer_manager.h"
#include "storage/on_disk/buffer_manager/page_buffer_manager_error.h"
#include "core/assert.h"
#include "storage/on_disk/buffer_manager/frame.h"
#include "storage/on_disk/buffer_manager/lru_k_replacer.h"
#include "storage/on_disk/buffer_manager/page.h"
#include "storage/on_disk/disk/disk_manager.h"
#include <chrono>
#include <mutex>
#include <stdlib.h>

PageBufferManager::PageBufferManager()
    : PageBufferManager(128)
{
}

PageBufferManager::PageBufferManager(std::size_t num_frames)
    : replacer_m()
    , disk_manager_m()
    , buffer_m(static_cast<char*>(malloc(num_frames * PAGE_SIZE)))
{
    YADB_ASSERT(buffer_m != nullptr, "Failed to allocate page buffer");
    for (frame_id_t id = 0; id < num_frames; id++) {
        MutFullPage data_view(reinterpret_cast<std::byte*>(buffer_m + id * PAGE_SIZE), PAGE_SIZE);
        frames_m.push_back(std::make_unique<Frame>(id, data_view));
        replacer_m.RegisterFrame(id);
    }
}

PageBufferManager::~PageBufferManager()
{
    for (auto& [fp_id, _] : page_map_m)
        FlushPage(fp_id);
    free(buffer_m);
}

std::expected<file_id_t, yadb::Error::Ptr> PageBufferManager::CreateFile()
{
    return disk_manager_m.CreateFile();
}

std::optional<yadb::Error::Ptr> PageBufferManager::CreateFile(file_id_t file_id)
{
    return disk_manager_m.CreateFile(file_id);
}

bool PageBufferManager::FileExists(file_id_t file_id) const
{
    return disk_manager_m.FileExists(file_id);
}

std::optional<yadb::Error::Ptr> PageBufferManager::DeleteFile(file_id_t file_id)
{
    std::unique_lock<std::mutex> lk(mut_m);

    bool released = available_frame_m.wait_for(lk, std::chrono::seconds(5),
        [this, file_id]() { return !FileHasPinnedPages(file_id); });

    if (!released)
        return std::make_shared<yadb::DeleteFileTimeoutError>(file_id);

    std::vector<file_page_id_t> to_evict;
    for (const auto& [fp_id, frame_id] : page_map_m) {
        if (fp_id.file_id == file_id)
            to_evict.push_back(fp_id);
    }
    for (const auto& fp_id : to_evict) {
        Frame* frame = frames_m[page_map_m.at(fp_id)].get();
        frame->is_dirty = false;
        page_map_m.erase(fp_id);
        replacer_m.SetEvictable(frame->id, true);
    }

    available_frame_m.notify_all();
    return disk_manager_m.DeleteFile(file_id);
}

std::expected<page_id_t, yadb::Error::Ptr> PageBufferManager::AllocatePage(file_id_t file_id)
{
    return disk_manager_m.AllocatePage(file_id);
}

std::expected<Page, yadb::Error::Ptr> PageBufferManager::GetPage(const file_page_id_t& fp_id)
{
    std::unique_lock<std::mutex> lk(mut_m);
    available_frame_m.wait(lk, [this, fp_id]() {
        return page_map_m.contains(fp_id) || replacer_m.GetEvictableCount() > 0;
    });

    if (!page_map_m.contains(fp_id)) {
        if (auto err = LoadPage(fp_id))
            return std::unexpected(err.value());
    }

    return PinAndReturnPage(fp_id);
}

std::expected<std::optional<Page>, yadb::Error::Ptr> PageBufferManager::GetPageIfFrameAvailable(const file_page_id_t& fp_id)
{
    std::unique_lock<std::mutex> lk(mut_m);

    if (!page_map_m.contains(fp_id) && replacer_m.GetEvictableCount() == 0)
        return std::optional<Page> {};

    if (!page_map_m.contains(fp_id)) {
        if (auto err = LoadPage(fp_id))
            return std::unexpected(err.value());
    }

    return std::optional<Page> { PinAndReturnPage(fp_id) };
}

std::optional<yadb::Error::Ptr> PageBufferManager::LoadPage(const file_page_id_t& fp_id)
{
    if (page_map_m.contains(fp_id))
        return std::nullopt;

    std::optional<frame_id_t> frame_id_opt = replacer_m.EvictFrame();
    if (!frame_id_opt)
        return std::make_shared<yadb::GetPageError>(fp_id, "no evictable frame available");

    Frame* frame = frames_m[*frame_id_opt].get();

    if (frame->is_dirty) {
        if (auto err = FlushPage(frame->fp_id))
            return err;
    }

    page_map_m.erase(frame->fp_id);

    if (auto err = disk_manager_m.ReadPage(fp_id, frame->data))
        return std::make_shared<yadb::GetPageError>(fp_id, (*err)->what());

    available_frame_m.notify_all();
    page_map_m[fp_id] = frame->id;
    frame->fp_id = fp_id;
    frame->is_dirty = false;
    frame->pin_count = 0;
    return std::nullopt;
}

bool PageBufferManager::FileHasPinnedPages(file_id_t file_id) const
{
    for (const auto& [fp_id, frame_id] : page_map_m) {
        if (fp_id.file_id == file_id && frames_m[frame_id]->pin_count.load(std::memory_order_acquire) > 0)
            return true;
    }
    return false;
}

std::optional<yadb::Error::Ptr> PageBufferManager::FlushPage(const file_page_id_t& fp_id)
{
    Frame* frame = GetFrameForPage(fp_id);
    if (!frame)
        return std::make_shared<yadb::FlushPageError>(fp_id, "page not in buffer pool");

    if (auto err = disk_manager_m.WritePage(fp_id, frame->data))
        return std::make_shared<yadb::FlushPageError>(fp_id, (*err)->what());

    return std::nullopt;
}

void PageBufferManager::RemoveAccessor(const file_page_id_t& fp_id)
{
    std::lock_guard<std::mutex> lk(mut_m);
    Frame* frame = GetFrameForPage(fp_id);
    YADB_ASSERT(frame != nullptr, "RemoveAccessor called for page not in buffer pool");

    auto prev = frame->pin_count.fetch_sub(1, std::memory_order_acq_rel);
    YADB_ASSERT(prev > 0, "RemoveAccessor called when pin_count == 0");

    if (prev == 1) {
        replacer_m.SetEvictable(frame->id, true);
        available_frame_m.notify_one();
    }
}

Frame* PageBufferManager::GetFrameForPage(const file_page_id_t& fp_id) const
{
    auto it = page_map_m.find(fp_id);
    if (it == page_map_m.end())
        return nullptr;
    return frames_m[it->second].get();
}

Page PageBufferManager::PinAndReturnPage(const file_page_id_t& fp_id)
{
    Frame* frame = GetFrameForPage(fp_id);
    YADB_ASSERT(frame != nullptr, "PinAndReturnPage called for page not in buffer pool");
    frame->pin_count.fetch_add(1, std::memory_order_acq_rel);
    replacer_m.RecordAccess(frame->id);
    replacer_m.SetEvictable(frame->id, false);
    return Page(this, frame);
}
