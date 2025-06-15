#pragma once

#include "common/type_definitions.h"
#include "spdlog/logger.h"
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <span>
#include <spdlog/sinks/basic_file_sink.h>
#include <string_view>
#include <unordered_set>

class DiskManager {
public:
    DiskManager(const std::filesystem::path& db_directory);
    DiskManager(const std::filesystem::path& db_directory, std::size_t page_capacity);
    ~DiskManager();

    page_id_t AllocatePage();
    bool WritePage(page_id_t page_id, PageView page_data);
    bool ReadPage(page_id_t page_id, MutPageView page_data);
    void DeletePage(page_id_t page_id);

private:
    static constexpr std::string_view DB_FILE_NAME = "data.db";
    static constexpr std::string_view DISK_MANAGER_LOG_FILE_NAME = "disk_manager.log";

    std::size_t GetOffset(page_id_t page_id);
    std::size_t GetDatabaseFileSize();

    std::size_t page_capacity_m;

    // list of pages that are considered free
    std::unordered_set<page_id_t> free_pages_m;

    std::fstream db_io_m;
    std::filesystem::path db_directory_m;
    std::shared_ptr<spdlog::logger> logger_m;
};
