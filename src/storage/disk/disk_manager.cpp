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

	std::filesystem::resize_file(db_file_m, page_capacity_m * PAGE_SIZE);
	assert(page_capacity * PAGE_SIZE == std::filesystem::file_size(db_file_m));
	for (int i = 0; i < page_capacity_m; i++)
	{
		free_pages_m.push_back(i * PAGE_SIZE);
	}

}

DiskManager::~DiskManager()
{
    db_io_m.close();
}

// maybe look returning an error code
void DiskManager::WritePage(page_id_t page_id, PageData page_data)
{
    std::size_t offset;
    if (offset_map_m.contains(page_id)) {
        offset = offset_map_m[page_id];
    } else {
        // TODO should this really be how I am handling it?
        offset = AllocatePage();
        offset_map_m[page_id] = offset;
    }

    db_io_m.seekg(offset);
    db_io_m.write(page_data.data(), page_data.size());
	assert(db_io_m.good());
}

// Read the contents of page data into page_data
void DiskManager::ReadPage(page_id_t page_id, PageData page_data)
{
    assert(offset_map_m.contains(page_id));
    size_t offset = offset_map_m[page_id];
    db_io_m.seekg(offset);
    db_io_m.read(page_data.data(), page_data.size());

    // check the codes to see if it failed...
}

void DiskManager::DeletePage(page_id_t page_id)
{
	if (not offset_map_m.contains(page_id))
		return;

	std::size_t offset = offset_map_m[page_id];
	offset_map_m.erase(page_id);
	free_pages_m.push_back(offset);
}

std::size_t DiskManager::AllocatePage()
{
    size_t offset;
    if (not free_pages_m.empty()) {
        offset = free_pages_m.back();
        free_pages_m.pop_back();
	} else if (page_capacity_m == 0) {
		page_capacity_m = 1;
		std::filesystem::resize_file(db_file_m, PAGE_SIZE);
		offset = 0;
    } else if (page_capacity_m > 0){
        if (offset_map_m.size() == page_capacity_m) {
            page_capacity_m *= 2;
            std::filesystem::resize_file(db_file_m, page_capacity_m * PAGE_SIZE);
        }
        offset = offset_map_m.size() * PAGE_SIZE;
    }
    return offset;
}
