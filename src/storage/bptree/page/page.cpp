#include "page/page.h"
#include "buffer/frame.h"
#include "buffer/page_buffer_manager.h"
#include "core/shared_spinlock.h"

Page::~Page()
{
    if (page_buffer_manager_m)
        page_buffer_manager_m->RemoveAccessor(frame_m->page_id);
}

Page::Page(PageBufferManager* page_buffer_manager, Frame* frame)
    : frame_m { frame }
    , page_buffer_manager_m { page_buffer_manager }
{
}

Page::Page(Page&& other)
    : frame_m { other.frame_m }
    , page_buffer_manager_m { other.page_buffer_manager_m }
{
    other.frame_m = nullptr;
    other.page_buffer_manager_m = nullptr;
}

Page& Page::operator=(Page&& other)
{
    if (&other != this) {
        frame_m = other.frame_m;
        other.frame_m = nullptr;
        page_buffer_manager_m = other.page_buffer_manager_m;
        other.page_buffer_manager_m = nullptr;
    }
    return *this;
}

void Page::lock()
{
    frame_m->mut.lock();
}

bool Page::try_lock()
{
    return frame_m->mut.try_lock();
}

void Page::unlock()
{
    frame_m->mut.unlock();
}

void Page::lock_shared()
{
    frame_m->mut.lock_shared();
}

bool Page::try_lock_shared()
{
    return frame_m->mut.try_lock_shared();
}

void Page::unlock_shared()
{
    frame_m->mut.unlock_shared();
}

MutFullPage Page::GetMutView() const
{
    return frame_m->data;
}

FullPage Page::GetView() const
{
    return frame_m->data;
}

page_id_t Page::GetPageId() const {
    return frame_m->page_id;
}
