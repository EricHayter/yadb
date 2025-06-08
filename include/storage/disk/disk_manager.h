#pragma once

#include "common/type_definitions.h"
#include <filesystem>
#include <fstream>
#include <list>
#include <span>
#include <unordered_set>

class DiskManager {
public:
    DiskManager(const std::filesystem::path& db_file);
    DiskManager(const std::filesystem::path& db_file, std::size_t page_capacity);
    ~DiskManager();

    page_id_t AllocatePage();
    void WritePage(page_id_t page_id, PageView page_data);
    void ReadPage(page_id_t page_id, MutPageView page_data);
	void DeletePage(page_id_t page_id);

private:
    std::size_t GetOffset(page_id_t page_id);
    std::size_t GetDatabaseFileSize();

    std::size_t page_capacity_m;

    // list of pages that are considered free
	std::unordered_set<page_id_t> free_pages_m;

    std::fstream db_io_m;
    std::filesystem::path db_file_m;
};
