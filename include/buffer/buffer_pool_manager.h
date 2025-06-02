#pragma once

#include "common/type_definitions.h"
#include "storage/disk/disk_scheduler.h"
#include "buffer/lru_k_replacer.h"

#include <filesystem>
#include <unordered_map>

class BufferPoolManager
{
	public:
	BufferPoolManager(std::size_t num_frames, const std::filesystem::path& db_file);
	page_id_t NewPage();
	void DeletePage(page_id_t page_id);
	void WritePage(page_id_t page_id, PageData page_data);
	void ReadPage(page_id_t page_id, PageData page_data);

	private:
	std::size_t num_frames_m;
	LRUKReplacer replacer_m;	
	DiskScheduler disk_scheduler_m;
	std::vector<char> buffer_m;
	std::vector<frame_id_t> free_frames_m;
	std::unordered_map<page_id_t, frame_id_t> page_map_m;
};
