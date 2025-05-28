#include "storage/disk/disk_manager.h"
#include "common/type_definitions.h"
#include <cassert>
#include <filesystem>

DiskManager::DiskManager(const std::filesystem::path& db_file)
{
    db_io_m = std::fstream(db_file);
    // TODO check to see if the open fails
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
	// TODO should this really be how I am handling it?
	} else {
		offset = AllocatePage();
		offset_map_m[page_id] = offset;
	}
	
	db_io_m.seekg(offset);
	db_io_m.write(page_data.data(), page_data.size());
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


std::size_t DiskManager::AllocatePage()
{
	size_t offset;
	if (not free_pages_m.empty())
	{
		offset = free_pages_m.back();
		free_pages_m.pop_back();
	} else {
		if (offset_map_m.size() >= page_capacity_m) {
        	page_capacity_m *= 2;
            std::filesystem::resize_file(db_file_path_m, (page_capacity_m) * PAGE_SIZE);
        }
		offset = offset_map_m.size() * PAGE_SIZE;
	}
	return offset;
}
