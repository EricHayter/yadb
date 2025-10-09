#include "storage/bptree/page/page.h"

#include <cassert>

Page::Page(PageBufferManager* buffer_manager, page_id_t page_id, MutPageView page_view, std::shared_lock<std::shared_mutex>&& lk)
    : BasePage(buffer_manager, page_id, page_view, false)
    , data_lk_m(std::move(lk))
{
    assert(data_lk_m.owns_lock());
}

Page::Page(Page&& other)
    : BasePage(std::move(other))
    , data_lk_m(std::move(other.data_lk_m))
{
}

Page& Page::operator=(Page&& other)
{
    if (&other != this) {
        data_lk_m = std::move(other.data_lk_m);
        BasePage::operator=(std::move(other));
    }
    return *this;
}
