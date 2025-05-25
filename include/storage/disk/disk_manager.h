#pragma once

#include "common/type_definitions.h"
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

class DiskManager {
public:
    DiskManager(const std::filesystem::path& db_file);

    // Write data to page to given id
    void WritePage(page_id_t page_id, const char* page_data);

    // Read contents of entire page
    char* ReadPage(page_id_t page_id);

private:
    std::size_t AllocatePage();

    std::size_t page_capacity_m { 10 }; // TODO fix this value to something else

    // map page id's to offsets in the database file
    std::unordered_map<page_id_t, std::size_t> offset_map_m;

    // list of offsets for free pages to use
    std::vector<std::size_t> free_pages_m;

    std::fstream db_io_m;
    std::filesystem::path db_file_path_m;
};
