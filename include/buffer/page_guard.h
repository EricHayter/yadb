#pragma once
#include "common/type_definitions.h"
#include <memory>
#include <shared_mutex>
#include <mutex>

class BufferPoolManager;
struct FrameHeader;

class ReadPageGuard
{
	public:
	ReadPageGuard(BufferPoolManager& buffer_pool_manager, FrameHeader &frame, std::shared_lock<std::shared_mutex> lk);
	ImutPageData GetData();

	private:
    BufferPoolManager& buffer_pool_manager_m;
    FrameHeader& frame_header_m;
    std::shared_lock<std::shared_mutex> lk_m;	
};

class WritePageGuard
{
	public:
	WritePageGuard(BufferPoolManager& buffer_pool_manager, FrameHeader &frame, std::unique_lock<std::shared_mutex> lk);
	PageData GetData();

	private:
    BufferPoolManager& buffer_pool_manager_m;
    FrameHeader& frame_header_m;
    std::shared_lock<std::shared_mutex> lk_m;	
};

