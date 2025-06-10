#include "storage/disk/disk_manager.h"
#include "common/type_definitions.h"
#include <cassert>
#include <filesystem>


DiskManager::DiskManager(const std::filesystem::path& db_file)
    : DiskManager(db_file, 1)
{
}

DiskManager::DiskManager(const std::filesystem::path& db_file, std::size_t page_capacity)
    : db_file_m(db_file)
    , page_capacity_m(page_capacity)
{
    // TODO worry about persistence later maybe?
    db_io_m.open(db_file_m, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    assert(db_io_m.is_open());

    // TODO check to see if this could fail
    std::filesystem::resize_file(db_file_m, GetDatabaseFileSize());
    assert(GetDatabaseFileSize() == std::filesystem::file_size(db_file_m));
    for (page_id_t id = 0; id < page_capacity_m; id++)
    {
        free_pages_m.insert(id);
    }

}

DiskManager::~DiskManager()
{
    db_io_m.close();
}

void DiskManager::WritePage(page_id_t page_id, PageView page_data)
{
    assert(page_id < page_capacity_m && not free_pages_m.contains(page_id));
    std::size_t offset = GetOffset(page_id);
    db_io_m.seekg(offset);
    db_io_m.write(page_data.data(), page_data.size());
    assert(db_io_m.good());
}

// Read the contents of page data into page_data
void DiskManager::ReadPage(page_id_t page_id, MutPageView page_data)
{
    assert(page_id < page_capacity_m && not free_pages_m.contains(page_id));
    size_t offset = GetOffset(page_id);
    db_io_m.seekg(offset);
    db_io_m.read(page_data.data(), page_data.size());
    assert(db_io_m.good());
}

void DiskManager::DeletePage(page_id_t page_id)
{
    // TODO might be worth to validate that this isn't a repeat?
    free_pages_m.insert(page_id);
}

page_id_t DiskManager::AllocatePage()
{
    page_id_t page_id;
    if (not free_pages_m.empty()) {
        auto iter = free_pages_m.begin();
        page_id = *iter;
        free_pages_m.erase(iter);
    } else {
        page_id = page_capacity_m;
        if (page_capacity_m == 0) {
            page_capacity_m = 1;
        } else {
            page_capacity_m *= 2;
        }
        std::filesystem::resize_file(db_file_m, GetDatabaseFileSize());

        // populate free page list with new pages
        for (int id = page_id + 1;  id < page_capacity_m;  id++)
            free_pages_m.insert(id);
    }
    return page_id;
}

std::size_t DiskManager::GetOffset(page_id_t page_id)
{
    return page_id * PAGE_SIZE;
}


std::size_t DiskManager::GetDatabaseFileSize()
{
    return page_capacity_m * PAGE_SIZE;
}
