#pragma once

#include "buffer/frame_header.h"
#include "buffer/lru_k_replacer.h"
#include "buffer/page_guard.h"
#include "common/type_definitions.h"
#include "storage/disk/disk_scheduler.h"
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <condition_variable>
#include <filesystem>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

class ReadPageGuard;
class WritePageGuard;

class PageBufferManager {
    friend ReadPageGuard;
    friend WritePageGuard;

public:
    PageBufferManager(const std::filesystem::path& db_directory, std::size_t num_frames);
    ~PageBufferManager();
    page_id_t NewPage();

    std::optional<ReadPageGuard> TryReadPage(page_id_t page_id);
    ReadPageGuard WaitReadPage(page_id_t page_id);

    std::optional<WritePageGuard> TryWritePage(page_id_t page_id);
    WritePageGuard WaitWritePage(page_id_t page_id);

private:
    bool LoadPage(page_id_t page_id);
    bool FlushPage(page_id_t page_id);
    void AddAccessor(frame_id_t frame_id, bool is_writer);
    void RemoveAccessor(frame_id_t frame_id);

private:
    static constexpr std::string_view PAGE_BUFFER_MANAGER_LOG_FILENAME{ "page_buffer_manager.log" };
    std::shared_ptr<spdlog::logger> logger_m;

    LRUKReplacer replacer_m;
    DiskScheduler disk_scheduler_m;

    char* buffer_m;

    std::unordered_map<page_id_t, frame_id_t> page_map_m;
    std::vector<std::unique_ptr<FrameHeader>> frames_m;

    std::mutex mut_m;
    std::condition_variable available_frame_m;
};
