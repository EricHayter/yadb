/*-----------------------------------------------------------------------------
 *
 * bptree.cpp
 *      B+ Tree implementation - Core functionality
 *
 * This file contains the core B+ tree functionality including constructors
 * and navigation methods. Search, insert, and delete operations are in
 * separate files for better organization.
 *
 *-----------------------------------------------------------------------------
 */

#include "tree/bptree.h"
#include "buffer/page_buffer_manager.h"
#include "page/page_iterator.h"
#include "page/page_layout.h"

#include <cassert>
#include <mutex>
#include <shared_mutex>

BPTree::BPTree(PageBufferManager* page_buffer_manager)
    : page_buffer_manager_m(page_buffer_manager)
{
    assert(page_buffer_manager_m);

    root_page_id_m = page_buffer_manager_m->AllocatePage();
    Page page = page_buffer_manager_m->GetPage(root_page_id_m);
    std::lock_guard<Frame> lg(*page.GetFrame());
    page.InitPage(PageType::BPTreeLeaf);
}

BPTree::BPTree(PageBufferManager* page_buffer_manager, page_id_t root_page_id)
    : page_buffer_manager_m(page_buffer_manager)
    , root_page_id_m(root_page_id)
{
    assert(page_buffer_manager_m);
}

// TODO fix this implementation to use shared lock all the way up to return value
// also I think it should be called from a context where the lock is already held...
template<typename LockType>
LockedPage<LockType> BPTree::NavigateToLeafImpl(std::span<const char> key)
{
    page_id_t current_page_id = root_page_id_m;

    // Start with root page
    Page current_page = page_buffer_manager_m->GetPage(current_page_id);
    LockType current_lock(*current_page.GetFrame());  // Lock the frame, not the page

    while (true) {
        PageType page_type = current_page.GetPageType();

        if (page_type == PageType::BPTreeLeaf) {
            return LockedPage<LockType>(std::move(current_page), std::move(current_lock));
        }
        else if (page_type == PageType::BPTreeInner) {
            // Inner node format: (key || child_page_id) pairs
            // Find the appropriate child pointer using FindPivotKey
            slot_id_t target_slot = FindPivotKey(current_page, key);
            page_id_t next_page_id = ReadInnerPageId(current_page, target_slot);

            // HAND-OVER-HAND: Get and lock child BEFORE releasing parent
            Page child_page = page_buffer_manager_m->GetPage(next_page_id);
            LockType child_lock(*child_page.GetFrame());  // Lock the frame, not the page

            // Now release parent lock
            current_lock.unlock();

            // Move to child
            current_page = std::move(child_page);
            current_lock = std::move(child_lock);
            // Loop continues with child as new current
        }
        else {
            assert(false && "Unexpected page type in B+ tree");
        }
    }
}

SharedLockedPage BPTree::NavigateToLeafShared(std::span<const char> key)
{
    return NavigateToLeafImpl<std::shared_lock<Frame>>(key);
}

ExclusiveLockedPage BPTree::NavigateToLeafExclusive(std::span<const char> key)
{
    return NavigateToLeafImpl<std::unique_lock<Frame>>(key);
}

// Explicit template instantiations
template LockedPage<std::shared_lock<Frame>> BPTree::NavigateToLeafImpl<std::shared_lock<Frame>>(std::span<const char> key);
template LockedPage<std::unique_lock<Frame>> BPTree::NavigateToLeafImpl<std::unique_lock<Frame>>(std::span<const char> key);
