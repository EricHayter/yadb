/*-----------------------------------------------------------------------------
 *
 * bptree_insert.cpp
 *      B+ Tree insertion and split operations
 *
 * This file contains the insertion logic including optimistic insert,
 * pessimistic insert with splits, and node splitting operations.
 *
 *-----------------------------------------------------------------------------
 */

#include "tree/bptree.h"
#include "buffer/page_buffer_manager.h"
#include "page/page_iterator.h"
#include "page/page_layout.h"

#include <cassert>
#include <algorithm>
#include <memory>
#include <mutex>
#include <stack>

void BPTree::Insert(std::span<const char> key, record_id_t record_id)
{
    if (!InsertOptimistic(key, record_id))
        InsertPessimistic(key, record_id);
}

bool BPTree::InsertOptimistic(std::span<const char> key, record_id_t record_id)
{
    auto locked_page = NavigateToLeafExclusive(key);
    uint16_t value_size = key.size() + sizeof(record_id);

    if (!locked_page.page.CanInsert(value_size))
        return false;

    InsertKey(locked_page.page, key, record_id);
    return true;
}

void BPTree::InsertPessimistic(std::span<const char> key, record_id_t record_id)
{
    /* We need to traverse all the way down to the leaf node holding all of the
     * locks all the way down (in case of needing to split somewhere above).
     * TODO improve this to only maintain the locks that are truly required.
     * I.e. it should be possible after a certain point to tell if a node
     * will be involved in any splitting.*/
    Page root_pg = page_buffer_manager_m->GetPage(root_page_id_m);
    std::unique_lock<Frame> root_pg_lk(*root_pg.GetFrame());

    std::stack<ExclusiveLockedPage> page_traversal;
    page_traversal.push(ExclusiveLockedPage(std::move(root_pg), std::move(root_pg_lk)));

    while (page_traversal.top().page.GetPageType() != PageType::BPTreeLeaf) {
        Page& page = page_traversal.top().page;
        slot_id_t pivot_slot_id = FindPivotKey(page, key);
        page_id_t child_pg_id = ReadInnerPageId(page, pivot_slot_id);

        Page next_page = page_buffer_manager_m->GetPage(child_pg_id);
        std::unique_lock<Frame> next_page_lk(*next_page.GetFrame());
        page_traversal.push(ExclusiveLockedPage(std::move(next_page), std::move(next_page_lk)));
    }

    /* if there isn't enough space in the leaf split it */
    uint16_t value_size = key.size() + sizeof(record_id_t);
    if (!page_traversal.top().page.CanInsert(value_size)) {
        page_id_t sibling_page_id = SplitNode(page_traversal);

        /* retry insert from scratch (simple approach) */
        while (!page_traversal.empty())
            page_traversal.pop();
        Insert(key, record_id);
        return;
    }

    Page leaf_page = std::move(page_traversal.top().page);
    InsertKey(leaf_page, key, record_id);
}

// TODO there is also the case of replacing the root page.
page_id_t BPTree::SplitNode(std::stack<ExclusiveLockedPage>& tree_traversal)
{
    /* This should be a helper function for the pages. It should already own
     * all of the pages that it needs. Calling function needs to hold the locks
     * */
    auto [page, lock] = std::move(tree_traversal.top());
    tree_traversal.pop();

    /* copy everything from the split key to the right into a new node
     * then copy the split key into the parent node. */
    PageIterator slot_iter = FindSplitKey(page);
    slot_id_t split_key_slot = *slot_iter;
    std::span<const char> split_key = ReadInnerKey(page, split_key_slot);

    /* create the sibling page */
    page_id_t sibling_page_id = page_buffer_manager_m->AllocatePage();
    Page sibling_page = page_buffer_manager_m->GetPage(sibling_page_id);
    std::unique_lock<Frame> sibling_lock(*sibling_page.GetFrame());
    sibling_page.InitPage(page.GetPageType());

    /* Move over all of the data after (and including) the split slot to the
     * new sibling pointer */
    for (auto iter = slot_iter; iter != page.end(); iter++) {
        slot_id_t slot_id = *sibling_page.AppendSlot(page.GetSlotSize(*iter));
        sibling_page.WriteSlot(slot_id, page.ReadSlot(*iter));
        page.DeleteSlot(*iter);
    }

    /* update sibling pointer for split page */
    if (page.GetPageType() == PageType::BPTreeLeaf) {
        slot_id_t slot_id = *page.AppendSlot(sizeof(sibling_page_id));
        std::unique_ptr<char[]> sibling_pointer_buffer = std::make_unique<char[]>(sizeof(sibling_page_id));
        std::memcpy(sibling_pointer_buffer.get(), &sibling_page_id, sizeof(sibling_page_id));
        page.WriteSlot(slot_id, { sibling_pointer_buffer.get(), sizeof(sibling_page_id) });
    }

    /* EDGE CASE: splitting root */
    if (tree_traversal.empty()) {
        CreateNewRoot(split_key, page.GetPageId(), sibling_page_id);
        return sibling_page_id;
    }

    /* Insert the split key into the parent node */
    Page& parent_page = tree_traversal.top().page;
    uint16_t split_key_size = page.GetSlotSize(split_key_slot);

    /* Parent page cannot contain the new key. Split the parent. */
    if (!parent_page.CanInsert(split_key_size)) {
        /* WARNING: invalidates parent_page */
        page_id_t parent_sibling_id = SplitNode(tree_traversal);

        /* get the left most key of the new page. To see if the parent node
         * has changed after the split in the parent. */
        Page parent_sibling = page_buffer_manager_m->GetPage(parent_sibling_id);
        std::shared_lock<Frame> parent_sibling_lock(*parent_sibling.GetFrame());

        /* update the parent of the child node potentially */
        slot_id_t left_most_slot_id = *parent_sibling.begin();
        auto left_most_key = ReadInnerKey(parent_sibling, left_most_slot_id);

        /* if the split key is larger than the smallest key in the parent's
         * sibling it must be under the parent's new sibling */
        if (std::ranges::lexicographical_compare(left_most_key, split_key)) {
            parent_page = std::move(parent_sibling);
        }
    }

    /* copy the key into the parent node. */
    InsertKey(parent_page, split_key, sibling_page_id);
    return sibling_page_id;
}

PageIterator BPTree::FindSplitKey(Page& page)
{
    /* Try and split so that roughly half of the total record size is
     * split between the two nodes */
    assert(page.GetPageType() == PageType::BPTreeInner || page.GetPageType() == PageType::BPTreeLeaf);

    int total_size = 0;
    for (auto iter = page.begin(); iter != page.end(); iter++) {
        total_size += page.GetSlotSize(*iter);
    }

    int partition_size = 0;
    for (auto iter = page.begin(); iter != page.end(); iter++) {
        partition_size += page.GetSlotSize(*iter);
        if (partition_size >= total_size / 2)
            return iter;
    }

    /* unreachable */
    assert(false && "Should couldn't find split key");
    return page.end();
}


page_id_t BPTree::CreateNewRoot(std::span<const char> key, page_id_t left_page_id, page_id_t right_page_id)
{
    /* Create a new root node with two children separated by the split key.
     *
     * B+ tree inner node format: slots contain [key || page_id] pairs.
     * For a new root with split_key:
     *   Slot 0: [split_key || left_page_id]  - keys < split_key go here
     *   Slot 1: ["" || right_page_id]        - keys >= split_key go here (empty key as sentinel)
     *
     * FindPivotKey logic: returns first slot where search_key < slot_key,
     * or last slot if search_key >= all keys.
     */
    page_id_t root_page_id = page_buffer_manager_m->AllocatePage();
    root_page_id_m = root_page_id;
    Page root_page = page_buffer_manager_m->GetPage(root_page_id);
    std::unique_lock<Frame> root_page_lock(*root_page.GetFrame());
    root_page.InitPage(PageType::BPTreeInner);

    /* Insert first entry: [split_key || left_page_id] */
    InsertKeyInner(root_page, key, left_page_id);

    /* Insert second entry: [empty_key || right_page_id]
     * Using empty key as sentinel for rightmost child */
    std::span<const char> empty_key;
    InsertKeyInner(root_page, empty_key, right_page_id);

    return root_page_id;
}
