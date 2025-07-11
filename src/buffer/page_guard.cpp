#include "buffer/page_guard.h"
#include "buffer/frame_header.h"
#include "buffer/page_buffer_manager.h"
#include <cassert>
#include <iostream>
#include <memory>

ReadPageGuard::ReadPageGuard(PageBufferManager* page_buffer_manager, FrameHeader* frame_header, std::shared_lock<std::shared_mutex>&& lk)
    : page_buffer_manager_m(page_buffer_manager)
    , frame_header_m(frame_header)
    , frame_lk_m(std::move(lk))
{
    assert(page_buffer_manager != nullptr);
    assert(frame_header != nullptr);

    // Creating a page guard is contingent on owning a lock to the frame's
    // shared lock. Otherwise, thread safety cannot be guaranteed, undermining
    // the job the guard itself.
    assert(frame_lk_m.owns_lock());
    page_buffer_manager_m->AddAccessor(frame_header_m->id, false);
}

ReadPageGuard::~ReadPageGuard()
{
    // no longer need access to lock since no more I/O can be performed on
    // deconstructed page guard...
    if (frame_lk_m)
        frame_lk_m.unlock();
    if (page_buffer_manager_m != nullptr)
        page_buffer_manager_m->RemoveAccessor(frame_header_m->id);
}

ReadPageGuard::ReadPageGuard(ReadPageGuard&& other)
    : page_buffer_manager_m(other.page_buffer_manager_m)
    , frame_header_m(other.frame_header_m)
    , frame_lk_m(std::move(other.frame_lk_m))
{
    other.page_buffer_manager_m = nullptr;
    other.frame_header_m = nullptr;
}

ReadPageGuard& ReadPageGuard::operator=(ReadPageGuard&& other)
{
    page_buffer_manager_m = other.page_buffer_manager_m;
    frame_header_m = other.frame_header_m;
    frame_lk_m = std::move(other.frame_lk_m);
    other.page_buffer_manager_m = nullptr;
    other.frame_header_m = nullptr;
    return *this;
}

PageView ReadPageGuard::GetData()
{
    return frame_header_m->GetData();
}

WritePageGuard::WritePageGuard::WritePageGuard(PageBufferManager* page_buffer_manager, FrameHeader* frame_header, std::unique_lock<std::shared_mutex>&& lk)
    : page_buffer_manager_m(page_buffer_manager)
    , frame_header_m(frame_header)
    , frame_lk_m(std::move(lk))
{
    assert(page_buffer_manager != nullptr);
    assert(frame_header != nullptr);

    // Creating a page guard is contingent on owning a lock to the frame's
    // shared lock. Otherwise, thread safety cannot be guaranteed, undermining
    // the job the guard itself.
    assert(frame_lk_m.owns_lock());
    page_buffer_manager_m->AddAccessor(frame_header_m->id, true);
}

WritePageGuard::~WritePageGuard()
{
    // no longer need access to lock since no more I/O can be performed on
    // deconstructed page guard...
    if (frame_lk_m)
        frame_lk_m.unlock();
    if (page_buffer_manager_m != nullptr)
        page_buffer_manager_m->RemoveAccessor(frame_header_m->id);
}

WritePageGuard::WritePageGuard(WritePageGuard&& other)
    : page_buffer_manager_m(other.page_buffer_manager_m)
    , frame_header_m(other.frame_header_m)
    , frame_lk_m(std::move(other.frame_lk_m))
{
    other.page_buffer_manager_m = nullptr;
    other.frame_header_m = nullptr;
}

WritePageGuard& WritePageGuard::operator=(WritePageGuard&& other)
{
    page_buffer_manager_m = other.page_buffer_manager_m;
    frame_header_m = other.frame_header_m;
    frame_lk_m = std::move(other.frame_lk_m);
    other.page_buffer_manager_m = nullptr;
    other.frame_header_m = nullptr;
    return *this;
}

MutPageView WritePageGuard::GetData()
{
    return frame_header_m->GetMutData();
}
