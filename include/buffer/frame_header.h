#pragma once
#include "common/type_definitions.h"
#include <atomic>
#include <shared_mutex>

struct FrameHeader
{
	MutPageView GetMutData();
	PageView GetData();
    const frame_id_t id;

        // Which page's data does this frame hold
	page_id_t page_id;

	// Has this page been altered?
	bool is_dirty;

	// concurrent readers/writers
	std::atomic<int> pin_count;

	// mutex for safely sharing access to the buffer
	std::shared_mutex mut;
};

