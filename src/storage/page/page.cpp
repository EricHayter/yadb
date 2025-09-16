#include "storage/page/page.h"

#include <cassert>

Page::Page(PageBufferManager* buffer_manager, page_id_t page_id, MutPageView page_view, std::shared_lock<std::shared_mutex>&& lk, bool fresh_page)
    : BasePage(buffer_manager, page_id, page_view, false, fresh_page)
    , data_lk_m(std::move(lk))
{
    assert(data_lk_m.owns_lock());
}
