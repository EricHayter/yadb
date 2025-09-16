#pragma once

#include "storage/page/base_page.h"

#include <shared_mutex>

/* Page handle for write access. Has all of the same functionality but also
 * support for updating page headers, slot directory entries, and writing data. */
class Page : public BasePage {
public:
    Page(PageBufferManager* buffer_manager, page_id_t page_id, MutPageView page_view, std::shared_lock<std::shared_mutex>&& lk, bool fresh_page);

    /* copy constructors will be disabled due to the data lock member */
    Page(const Page& other) = delete;
    Page& operator=(const Page& other) = delete;

private:
    /* lock for ownership of page data in Page parent class */
    std::shared_lock<std::shared_mutex> data_lk_m;
};
