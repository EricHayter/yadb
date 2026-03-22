#include "storage/on_disk/heap/heap_page.h"
#include "common/definitions.h"
#include "storage/on_disk/page/page_format.h"
#include <cstring>

namespace heap_page {

void InitPage(MutFullPage page)
{
    page::InitPage(page, page::PageType::Data);

    page::SetEndFreeSpace(page, page::GetEndFreeSpace(FullPage(page)) - sizeof(page_id_t) * 2);
    SetNextPage(page, NULL_PAGE_ID);
    SetPrevPage(page, NULL_PAGE_ID);
}

void SetNextPage(MutFullPage page, page_id_t page_id)
{
    std::memcpy(
        page.data() + PAGE_SIZE - sizeof(page_id),
        &page_id,
        sizeof(page_id));
}

void SetPrevPage(MutFullPage page, page_id_t page_id)
{
    std::memcpy(
        page.data() + PAGE_SIZE - sizeof(page_id) * 2,
        &page_id,
        sizeof(page_id));
}

page_id_t GetNextPage(FullPage page)
{
    page_id_t page_id;
    std::memcpy(
        &page_id,
        page.data() + PAGE_SIZE - sizeof(page_id),
        sizeof(page_id));
    return page_id;
}

page_id_t GetPrevPage(FullPage page)
{
    page_id_t page_id;
    std::memcpy(
        &page_id,
        page.data() + PAGE_SIZE - sizeof(page_id) * 2,
        sizeof(page_id));
    return page_id;
}

};
