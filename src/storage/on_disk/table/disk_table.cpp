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

DiskTable::DiskTable(file_id_t file_id, const Schema& schema, PageBufferManager& page_buffer_manager)
    : Table(schema)
    , file_id_m (file_id)
    , page_buffer_manager_m(page_buffer_manager)
{
}

std::unique_ptr<TableIterator> DiskTable::iter()
{
    return std::make_unique<DiskTableIterator>(file_id_m, page_buffer_manager_m);
}

row_id_t DiskTable::insert_row_impl(std::span<const std::byte> row)
{
    page_id_t current_page_id = ROOT_PAGE_ID;
    page_id_t next_page_id = NULL_PAGE_ID;

    Page page = MustGetPage(page_buffer_manager_m,{ file_id_m, ROOT_PAGE_ID });
    std::unique_lock<Page> lk(page);
    next_page_id = heap_page::GetPartialPagesHead(page.GetView());

    // Finding a place for insert
    while (next_page_id != NULL_PAGE_ID) {
        Page next_page = MustGetPage(page_buffer_manager_m,{ file_id_m, next_page_id });
        std::unique_lock<Page> new_lk(next_page);
        std::swap(new_lk, lk);
        std::swap(page, next_page);
        std::optional<slot_id_t> insert_loc = page::AllocateSlot(page.GetMutView(), row.size());
        if (insert_loc) {
            auto slot_span = page::WriteRecord(page.GetMutView(), *insert_loc);
            std::copy(row.begin(), row.end(), slot_span.begin());
            return MakeRowId(page.GetFilePageId().page_id, *insert_loc);
        }

        current_page_id = next_page_id;
        next_page_id = heap_page::GetNextPage(page.GetView());
    }

    row_id_t inserted_row_id;
    auto alloc = page_buffer_manager_m.AllocatePage(file_id_m);
    YADB_ASSERT(alloc.has_value(), alloc.error()->what().c_str());
    page_id_t new_page_id = *alloc;
    // create new node in the linked list
    {
        Page new_page = MustGetPage(page_buffer_manager_m,{ file_id_m, new_page_id });
        std::lock_guard<Page> lg(new_page);
        heap_page::InitPage(new_page.GetMutView());
        heap_page::SetPrevPage(new_page.GetMutView(), current_page_id);

        std::optional<slot_id_t> insert_loc = page::AllocateSlot(new_page.GetMutView(), row.size());
        auto slot_span = page::WriteRecord(new_page.GetMutView(), *insert_loc);
        std::copy(row.begin(), row.end(), slot_span.begin());
        inserted_row_id = MakeRowId(new_page_id, *insert_loc);
    }

    // update pointers
    heap_page::SetNextPage(page.GetMutView(), new_page_id);

    return inserted_row_id;
}

row_id_t DiskTable::update_row(const row_id_t& rid, std::span<const std::byte> data)
{
    delete_row(rid);
    return insert_row_impl(data);
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
