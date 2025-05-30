#pragma once
#include "common/type_definitions.h"
#include <shared_mutex>
#include <array>
#include <atomic>

class FrameHandle {
	public:
	FrameHandle(frame_id_t frame_id) : frame_id_m(frame_id) {}
	PageData GetData() { return data_m; };
	
	private:
	const frame_id_t frame_id_m;

	// Using a shared_mutex to allow for thread-safe reads from multiple
	// sources while have mutually exclusive writes.
	std::shared_mutex io_mut_m;

	std::atomic<std::size_t> access_count;

	// Has the frame been written to without writing the changes to disk?
	bool is_dirty_m;
	std::array<char, PAGE_SIZE> data_m;
};
