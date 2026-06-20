#include "storage/on_disk/table/disk_table.h"
#include "storage/on_disk/table/disk_table_iterator.h"
#include "storage/on_disk/heap/heap_page.h"
#include "storage/on_disk/page/page_format.h"
#include "storage/on_disk/types.h"
#include "storage/on_disk/constants.h"
#include "core/assert.h"

// Helper to extract a Page from GetPage, asserting on IO failure.
// TODO: propagate errors once the Table interface supports it.
static Page MustGetPage(PageBufferManager& pbm, file_page_id_t fp_id)
{
    auto result = pbm.GetPage(fp_id);
    YADB_ASSERT(result.has_value(), result.error()->what().c_str());
    return std::move(*result);
}

DiskTable::DiskTable(file_id_t file_id, PageBufferManager& page_buffer_manager)
    : file_id_m(file_id)
    , page_buffer_manager_m(page_buffer_manager)
{
}

TableCursor DiskTable::begin()
{
    return TableCursor { std::make_unique<DiskTableIterator>(file_id_m, page_buffer_manager_m) };
}

row_id_t DiskTable::insert_row(std::span<const std::byte> row)
{
    // Read the head of the partial list (pages with room) off the root.
    page_id_t next_page_id;
    {
        Page root = MustGetPage(page_buffer_manager_m, { file_id_m, ROOT_PAGE_ID });
        std::lock_guard<Page> lg(root);
        next_page_id = heap_page::GetPartialPagesHead(root.GetView());
    }

    // Walk the partial list one page at a time looking for room.
    page_id_t last_partial_id = NULL_PAGE_ID;
    while (next_page_id != NULL_PAGE_ID) {
        Page page = MustGetPage(page_buffer_manager_m, { file_id_m, next_page_id });
        std::lock_guard<Page> lg(page);

        std::optional<slot_id_t> insert_loc = page::AllocateSlot(page.GetMutView(), row.size());
        if (insert_loc) {
            auto slot_span = page::WriteRecord(page.GetMutView(), *insert_loc);
            std::copy(row.begin(), row.end(), slot_span.begin());
            return MakeRowId(next_page_id, *insert_loc);
        }

        last_partial_id = next_page_id;
        next_page_id = heap_page::GetNextPage(page.GetView());
    }

    // No partial page had room: allocate a new page and write the row.
    auto alloc = page_buffer_manager_m.AllocatePage(file_id_m);
    YADB_ASSERT(alloc.has_value(), alloc.error()->what().c_str());
    page_id_t new_page_id = *alloc;

    row_id_t inserted_row_id;
    {
        Page new_page = MustGetPage(page_buffer_manager_m, { file_id_m, new_page_id });
        std::lock_guard<Page> lg(new_page);
        heap_page::InitPage(new_page.GetMutView());

        std::optional<slot_id_t> insert_loc = page::AllocateSlot(new_page.GetMutView(), row.size());
        auto slot_span = page::WriteRecord(new_page.GetMutView(), *insert_loc);
        std::copy(row.begin(), row.end(), slot_span.begin());
        inserted_row_id = MakeRowId(new_page_id, *insert_loc);
    }

    // Link the new page into the partial list so later inserts reuse it and the
    // iterator reaches it: it becomes the head (the root's partial-list pointer)
    // when the list was empty, otherwise it is appended after the last page.
    if (last_partial_id == NULL_PAGE_ID) {
        Page root = MustGetPage(page_buffer_manager_m, { file_id_m, ROOT_PAGE_ID });
        std::lock_guard<Page> lg(root);
        heap_page::SetPrevPage(root.GetMutView(), new_page_id);
    } else {
        Page last_page = MustGetPage(page_buffer_manager_m, { file_id_m, last_partial_id });
        std::lock_guard<Page> lg(last_page);
        heap_page::SetNextPage(last_page.GetMutView(), new_page_id);
    }

    return inserted_row_id;
}

row_id_t DiskTable::update_row(const row_id_t& rid, std::span<const std::byte> data)
{
    delete_row(rid);
    return insert_row(data);
}

void DiskTable::delete_row(const row_id_t& rid)
{
    page_id_t page_id = GetPageIdFromRowId(rid);
    slot_id_t slot_id = GetSlotIdFromRowId(rid);

    Page page = MustGetPage(page_buffer_manager_m,{ file_id_m, page_id });
    std::lock_guard<Page> lg(page);
    page::DeleteSlot(page.GetMutView(), slot_id);
}

TableType DiskTable::GetType() const
{
    return TableType::Disk;
}


std::filesystem::path DiskTable::GetTableFileName(std::string_view table_name)
{
    return std::string(table_name) + ".db";
}
