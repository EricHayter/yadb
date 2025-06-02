#include "buffer/buffer_pool_manager.h"
#include "common/type_definitions.h"

BufferPoolManager::BufferPoolManager(std::size_t num_frames, const std::filesystem::path& db_file) :
	num_frames_m(num_frames), 
	disk_scheduler_m(db_file),
	replacer_m(),
	buffer_m(num_frames * PAGE_SIZE)
{}

page_id_t BufferPoolManager::NewPage()
{
	throw "Not implemented yet";
}

void BufferPoolManager::BufferPoolManager::DeletePage(page_id_t page_id)
{
	throw "Not implemented yet";
}

void BufferPoolManager::WritePage(page_id_t page_id, PageData page_data)
{
	throw "Not implemented yet";
}

void BufferPoolManager::ReadPage(page_id_t page_id, PageData page_data)
{
	throw "Not implemented yet";
}


