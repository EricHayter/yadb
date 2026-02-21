#pragma once

#include "common/definitions.h"
#include "storage/bptree/buffer_manager/page_buffer_manager.h"
#include "storage/bptree/buffer_manager/page.h"
#include <functional>
#include <vector>

using PageSlotPair = std::pair<Page*, slot_id_t>;

/* a run of pages (i.e. set of pages that sorted in order */
using Run = std::vector<page_id_t>;

constexpr size_t MAX_SORT_POOL_SIZE = 2048;

// Returns true if the first argument should come before the second argument in a proper ordering
using RecordComparisonFunction = std::function<bool (PageSlice, PageSlice)>;

std::vector<page_id_t> SortPages(const PageBufferManager& page_buffer_manager, const std::vector<page_id_t>& pages, RecordComparisonFunction& func);


void SortPageInPlace(Page& page, RecordComparisonFunction& func);
void SortPageInPlace(Page& page, RecordComparisonFunction& func, slot_id_t left_bound, slot_id_t right_bound);
void SwapSlots(Page& page, slot_id_t slot1, slot_id_t slot2);

void ShiftSlotsLeft(Page& page);
