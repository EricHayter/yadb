#pragma once

#include "common/type_definitions.h"
#include <filesystem>
#include <fstream>
#include <list>
#include <span>
#include <unordered_map>


class DiskManager {
public:
    DiskManager(const std::filesystem::path& db_file);
    ~DiskManager();

    // Write data to page to given id
    void WritePage(page_id_t page_id, PageData page_data);

    // Read contents of entire page
    void ReadPage(page_id_t page_id, PageData page_data);

private:
    std::size_t AllocatePage();

    std::size_t page_capacity_m { 8 };

    // map page id's to offsets in the database file
    std::unordered_map<page_id_t, std::size_t> offset_map_m;

    // list of offsets for free pages to use
    std::list<std::size_t> free_pages_m;

    std::fstream db_io_m;
    std::filesystem::path db_file_path_m;
};
