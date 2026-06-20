#pragma once

#include "table/table_iterator.h"
#include "storage/on_disk/buffer_manager/page_buffer_manager.h"
#include "storage/on_disk/types.h"
#include <vector>

class DiskTableIterator : public TableIterator {
public:
    DiskTableIterator(file_id_t file_id, PageBufferManager& pbm);
    TableIterator::value_type operator*() const override;
    TableIterator& operator++() override;
    bool operator==(std::default_sentinel_t) const override;
    void seek(row_id_t rid) override;

private:
    void advance_to_next_valid();

    file_id_t file_id_m;
    PageBufferManager& page_buffer_manager_m;

    page_id_t current_page_id_m;
    slot_id_t current_slot_m;
    page_id_t partial_pages_head_m;
    bool in_partial_list_m;

    // Owns the bytes of the current row. The page it was read from is only
    // pinned momentarily during advance, so we copy the record here to back the
    // span handed out by operator*. Valid until the next advance.
    std::vector<std::byte> row_buffer_m;
};
