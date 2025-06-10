#include "buffer/page_guard.h"
#include "buffer/buffer_pool_manager.h"
#include "buffer/frame_header.h"
#include <cassert>
#include <iostream>
#include <memory>

ReadPageGuard::ReadPageGuard(BufferPoolManager* buffer_pool_manager, FrameHeader* frame_header, std::shared_lock<std::shared_mutex>&& lk)
    : buffer_pool_manager_m(buffer_pool_manager)
    , frame_header_m(frame_header)
    , lk_m(std::move(lk))
{
    assert(buffer_pool_manager != nullptr);
    assert(frame_header != nullptr);

    // Creating a page guard is contingent on owning a lock to the frame's
    // shared lock. Otherwise, thread safety cannot be guaranteed, undermining
    // the job the guard itself.
    assert(lk_m.owns_lock());
}

ReadPageGuard::~ReadPageGuard()
{
    if (buffer_pool_manager_m != nullptr)
        buffer_pool_manager_m->RemoveAccessor(frame_header_m->id);
}

ReadPageGuard::ReadPageGuard(ReadPageGuard&& other)
    : buffer_pool_manager_m(other.buffer_pool_manager_m)
    , frame_header_m(other.frame_header_m)
    , lk_m(std::move(other.lk_m))
{
    other.buffer_pool_manager_m = nullptr;
    other.frame_header_m = nullptr;
}

ReadPageGuard& ReadPageGuard::operator=(ReadPageGuard&& other)
{
    buffer_pool_manager_m = other.buffer_pool_manager_m;
    frame_header_m = other.frame_header_m;
    lk_m = std::move(other.lk_m);
    other.buffer_pool_manager_m = nullptr;
    other.frame_header_m = nullptr;
    return *this;
}

PageView ReadPageGuard::GetData()
{
    return frame_header_m->GetData();
}

WritePageGuard::WritePageGuard::WritePageGuard(BufferPoolManager* buffer_pool_manager, FrameHeader* frame_header, std::unique_lock<std::shared_mutex>&& lk)
    : buffer_pool_manager_m(buffer_pool_manager)
    , frame_header_m(frame_header)
    , lk_m(std::move(lk))
{
    assert(buffer_pool_manager != nullptr);
    assert(frame_header != nullptr);

    // Creating a page guard is contingent on owning a lock to the frame's
    // shared lock. Otherwise, thread safety cannot be guaranteed, undermining
    // the job the guard itself.
    assert(lk_m.owns_lock());
}

WritePageGuard::~WritePageGuard()
{
    if (buffer_pool_manager_m != nullptr)
        buffer_pool_manager_m->RemoveAccessor(frame_header_m->id);
}

WritePageGuard::WritePageGuard(WritePageGuard&& other)
    : buffer_pool_manager_m(other.buffer_pool_manager_m)
    , frame_header_m(other.frame_header_m)
    , lk_m(std::move(other.lk_m))
{
    other.buffer_pool_manager_m = nullptr;
    other.frame_header_m = nullptr;
}

WritePageGuard& WritePageGuard::operator=(WritePageGuard&& other)
{
    buffer_pool_manager_m = other.buffer_pool_manager_m;
    frame_header_m = other.frame_header_m;
    lk_m = std::move(other.lk_m);
    other.buffer_pool_manager_m = nullptr;
    other.frame_header_m = nullptr;
    return *this;
}

MutPageView WritePageGuard::GetData()
{
    return frame_header_m->GetMutData();
}
