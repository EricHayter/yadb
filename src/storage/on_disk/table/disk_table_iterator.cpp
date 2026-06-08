#include "storage/on_disk/table/disk_table_iterator.h"
#include "core/assert.h"
#include "storage/on_disk/constants.h"
#include "storage/on_disk/heap/heap_page.h"
#include "storage/on_disk/page/page_format.h"
#include <shared_mutex>

static Page MustGetPage(PageBufferManager& pbm, file_page_id_t fp_id)
{
    auto result = pbm.GetPage(fp_id);
    YADB_ASSERT(result.has_value(), result.error()->what().c_str());
    return std::move(*result);
}

DiskTableIterator::DiskTableIterator(file_id_t file_id, PageBufferManager& pbm)
    : file_id_m(file_id)
    , page_buffer_manager_m(pbm)
    , current_page_id_m(NULL_PAGE_ID)
    , current_slot_m(0)
    , partial_pages_head_m(NULL_PAGE_ID)
    , in_partial_list_m(false)
{
    Page root = MustGetPage(page_buffer_manager_m,{ file_id_m, ROOT_PAGE_ID });
    std::shared_lock<Page> lk(root);
    page_id_t full_head = heap_page::GetFullPagesHead(root.GetView());
    partial_pages_head_m = heap_page::GetPartialPagesHead(root.GetView());
    lk.unlock();

    if (full_head != NULL_PAGE_ID) {
        current_page_id_m = full_head;
    } else if (partial_pages_head_m != NULL_PAGE_ID) {
        current_page_id_m = partial_pages_head_m;
        in_partial_list_m = true;
    }

    advance_to_next_valid();
}

TableIterator& DiskTableIterator::operator++()
{
    current_slot_m++;
    advance_to_next_valid();
    return *this;
}

void DiskTableIterator::seek(row_id_t rid)
{
    page_id_t page_id = GetPageIdFromRowId(rid);
    slot_id_t slot_id = GetSlotIdFromRowId(rid);

    Page page = MustGetPage(page_buffer_manager_m,{ file_id_m, page_id });
    std::shared_lock<Page> lk(page);
    auto view = page.GetView();

    if (slot_id >= page::GetPageCapacity(view) || page::IsSlotDeleted(view, slot_id))
        throw std::invalid_argument("Invalid row_id in seek");

    auto record = page::ReadRecord(view, slot_id);
    row_buffer_m.assign(record.begin(), record.end());
    current_page_id_m = page_id;
    current_slot_m = slot_id;
    current_m = Row(rid, std::span<const std::byte>(row_buffer_m.data(), row_buffer_m.size()));
}

void DiskTableIterator::advance_to_next_valid()
{
    while (current_page_id_m != NULL_PAGE_ID) {
        page_id_t next_page_id = NULL_PAGE_ID;

        {
            Page page = MustGetPage(page_buffer_manager_m,{ file_id_m, current_page_id_m });
            std::shared_lock<Page> lk(page);
            auto view = page.GetView();
            uint16_t capacity = page::GetPageCapacity(view);

            while (current_slot_m < capacity) {
                if (!page::IsSlotDeleted(view, current_slot_m)) {
                    auto record = page::ReadRecord(view, current_slot_m);
                    row_buffer_m.assign(record.begin(), record.end());
                    current_m = Row(
                        MakeRowId(current_page_id_m, current_slot_m),
                        std::span<const std::byte>(row_buffer_m.data(), row_buffer_m.size()));
                    return;
                }
                current_slot_m++;
            }

            next_page_id = heap_page::GetNextPage(view);
        }

        if (next_page_id != NULL_PAGE_ID) {
            current_page_id_m = next_page_id;
        } else if (!in_partial_list_m && partial_pages_head_m != NULL_PAGE_ID) {
            in_partial_list_m = true;
            current_page_id_m = partial_pages_head_m;
        } else {
            current_page_id_m = NULL_PAGE_ID;
        }
        current_slot_m = 0;
    }

    current_m = std::nullopt;
}
