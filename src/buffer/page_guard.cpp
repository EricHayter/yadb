#include "buffer/page_guard.h"
#include <cassert>

ReadPageGuard::ReadPageGuard(BufferPoolManager& buffer_pool_manager, FrameHeader& frame, std::shared_lock<std::shared_mutex>&& lk)
	: buffer_pool_manager_m(buffer_pool_manager)
	, frame_header_m(frame)
	  , lk_m(std::move(lk))
{
	// Creating a page guard is contingent on owning a lock to the frame's
	// shared lock. Otherwise, thread safety cannot be guaranteed, undermining
	// the job the guard itself.
	assert(lk_m.owns_lock());
}

ReadPageGuard::~ReadPageGuard()
{
	buffer_pool_manager_m.RemoveAccessor(frame_header_m.id);
}

PageView ReadPageGuard::GetData()
{
	return frame_header_m.GetData();
}

WritePageGuard::WritePageGuard(BufferPoolManager& buffer_pool_manager, FrameHeader &frame, std::unique_lock<std::shared_mutex>&& lk)
	: buffer_pool_manager_m(buffer_pool_manager)
	, frame_header_m(frame)
	, lk_m(std::move(lk))
{
    // Creating a page guard is contingent on owning a lock to the frame's
    // shared lock. Otherwise, thread safety cannot be guaranteed, undermining
    // the job the guard itself.
    assert(lk_m.owns_lock);
}

WritePageGuard::~WritePageGuard()
{
	buffer_pool_manager_m.RemoveAccessor(frame_header_m.id);
}

MutPageView WritePageGuard::GetData()
{
	return frame_header_m.GetMutData();
}
