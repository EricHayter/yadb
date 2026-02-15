#include "optimizer/external_sort.h"
#include "common/definitions.h"
#include "storage/slotted_page/page_format.h"
#include <algorithm>
#include <shared_mutex>

//std::vector<Page> SortPages(PageBufferManager& page_buffer_manager, std::vector<Page>& pages, RecordComparisonFunction& func)
//{
//
//    // Run 0
//    int desired_page_count = std::max(MAX_SORT_POOL_SIZE, pages.size());
//
//    std::vector<Run> runs;
//
//    Run run;
//    for (int i = 0; i < desired_page_count; i++) {
//        page_id_t page_id = page_buffer_manager.AllocatePage();
//        auto page_opt = page_buffer_manager.GetPageIfFrameAvailable(page_id);
//        if (!page_opt.has_value()) {
//            // delete the newly allocated page (TODO add this to page buffer manger interface)
//            break;
//        }
//
//        // maybe just copy over the data then sort?
//        Page page = std::move(page_opt.value());
//        Page& input_page = pages[i];
//        std::shared_lock<Page> lk(input_page);
//        std::copy(input_page.GetView().begin(), input_page.GetView().end(), page.GetMutView().begin());;
//        run.push_back(std::move(page));
//    }
//    //sort the run
//
//
//    return {};
//}

void SortPageInPlace(Page& page, RecordComparisonFunction& func)
{
    ShiftSlotsLeft(page);
    SortPageInPlace(page, func, 0, page::GetNumTuples(page.GetView()));
}

void SortPageInPlace(Page& page, RecordComparisonFunction& comp, slot_id_t left_bound, slot_id_t right_bound)
{
    using namespace page;
    if (right_bound - left_bound <= 1)
        return;

    slot_id_t partition_ptr = right_bound - 1;
    slot_id_t lt_ptr = left_bound;
    slot_id_t gt_ptr = left_bound;

    /* find the first element that is after than the partition (based on the
     * ordering defined by comp. */
    while (gt_ptr < partition_ptr && comp(ReadRecord(page.GetView(), gt_ptr), ReadRecord(page.GetView(), partition_ptr))) {
        gt_ptr++;
        lt_ptr++;
    }

    /* move over all elements that are before (based on the ordering of comp)
     * to the left of all of the elements after the partition */
    while (lt_ptr < partition_ptr) {
        /* move the lt_ptr to find an element that is before partition in
         * comp's ordering. */
        while (lt_ptr < partition_ptr && !comp(ReadRecord(page.GetView(), lt_ptr), ReadRecord(page.GetView(), partition_ptr))) {
                lt_ptr++;
        }

        if (lt_ptr < partition_ptr) {
            SwapSlots(page, lt_ptr, gt_ptr);
            gt_ptr++;
            lt_ptr++;
        }
    }

    /* Move the pivot to its final position */
    SwapSlots(page, gt_ptr, partition_ptr);

    SortPageInPlace(page, comp, left_bound, gt_ptr);
    SortPageInPlace(page, comp, gt_ptr + 1, right_bound);
}

void SwapSlots(Page& page, slot_id_t slot1, slot_id_t slot2)
{
    using namespace page;
    offset_t temp_offset = GetSlotOffset(page.GetView(), slot1);
    size_t temp_size = GetSlotSize(page.GetView(), slot1);
    bool temp_is_deleted = IsSlotDeleted(page.GetView(), slot1);

    SetSlotOffset(page.GetMutView(), slot1, GetSlotOffset(page.GetView(), slot2));
    SetSlotSize(page.GetMutView(), slot1, GetSlotSize(page.GetView(), slot2));
    SetSlotDeleted(page.GetMutView(), slot1, IsSlotDeleted(page.GetView(), slot2));

    SetSlotOffset(page.GetMutView(), slot2, temp_offset);
    SetSlotSize(page.GetMutView(), slot2, temp_size);
    SetSlotDeleted(page.GetMutView(), slot2, temp_is_deleted);
}

void ShiftSlotsLeft(Page& page)
{
    slot_id_t deleted_ptr = 0;
    slot_id_t ptr = 0;

    while (ptr < page::GetPageCapacity(page.GetView()) && !page::IsSlotDeleted(page.GetView(), ptr)) {
        deleted_ptr++;
        ptr++;
    }

    while (deleted_ptr < page::GetNumTuples(page.GetView())) {
        while (page::IsSlotDeleted(page.GetView(), ptr))
            ptr++;

        /* copy over the slot entry data into the deleted slot on the left */
        page::SetSlotDeleted(page.GetMutView(), deleted_ptr, false);
        page::SetSlotOffset(page.GetMutView(), deleted_ptr, page::GetSlotOffset(page.GetView(), ptr));
        page::SetSlotSize(page.GetMutView(), deleted_ptr, page::GetSlotSize(page.GetView(), ptr));
        page::SetSlotDeleted(page.GetMutView(), ptr, true);
        page::SetSlotOffset(page.GetMutView(), ptr, 0);
        page::SetSlotSize(page.GetMutView(), ptr, 0);

        deleted_ptr++;
        ptr++;
    }
}
