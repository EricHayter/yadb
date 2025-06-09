#pragma once
#include "buffer/buffer_pool_manager.h"
#include "buffer/frame_header.h"
#include "common/type_definitions.h"
#include <memory>
#include <mutex>
#include <shared_mutex>

/* ============================================================================
 * Page Guards
 * ============================================================================
 * To allow for concurrent access to frames in the buffer pool manager, page
 * guards allow for a thread safe view into the buffers. This is done using
 * a shared_lock allowing multiple readers at once to a frame OR a single
 * writer. Both page guards should be responsible for automatically interacting
 * with the buffer pool manager to indicate when access has been gained and
 * released to a page (needed for the replacement policy).
 */


class BufferPoolManager;

class ReadPageGuard
{
public:
	ReadPageGuard(BufferPoolManager& buffer_pool_manager, FrameHeader &frame, std::shared_lock<std::shared_mutex>&& lk);
	~ReadPageGuard();
	ReadPageGuard(ReadPageGuard&& other);
	ReadPageGuard& operator=(ReadPageGuard&& other);

	PageView GetData();

private:
	std::shared_ptr<BufferPoolManager> buffer_pool_manager_m;
	std::shared_ptr<FrameHeader> frame_header_m;
    std::shared_lock<std::shared_mutex> lk_m;
};

class WritePageGuard
{
public:
	WritePageGuard(BufferPoolManager& buffer_pool_manager, FrameHeader &frame, std::unique_lock<std::shared_mutex>&& lk);
	~WritePageGuard();
	WritePageGuard(WritePageGuard&& other);
	WritePageGuard& operator=(WritePageGuard&& other);

	MutPageView GetData();

private:
	std::shared_ptr<BufferPoolManager> buffer_pool_manager_m;
	std::shared_ptr<FrameHeader> frame_header_m;
    std::unique_lock<std::shared_mutex> lk_m;
};

