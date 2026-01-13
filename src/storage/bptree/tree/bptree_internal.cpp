/*-----------------------------------------------------------------------------
 *
 * bptree_internal.cpp
 *      B+ Tree internal helper functions
 *
 * This file contains internal helper functions used by the B+ tree operations:
 * - Key/value reading functions (ReadLeafKey, ReadInnerPageId, etc.)
 * - Search helper functions (FindPivotKey, FindKey, FindInsertPosition)
 * - Key insertion helpers (InsertKey template)
 *
 *-----------------------------------------------------------------------------
 */

#include "tree/bptree.h"
#include "page/page_iterator.h"
#include "page/page_layout.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

/*-----------------------------------------------------------------------------
 * Key/Value Reading Functions
 *-----------------------------------------------------------------------------
 */

record_id_t BPTree::ReadLeafRecordId(Page& page, slot_id_t slot_id)
{
    std::span<const char> record = page.ReadSlot(slot_id);
    record_id_t record_id;
    std::memcpy(&record_id, record.data() + record.size() - sizeof(record_id_t), sizeof(record_id_t));
    return record_id;
}

std::span<const char> BPTree::ReadLeafKey(Page& page, slot_id_t slot_id)
{
    std::span<const char> record = page.ReadSlot(slot_id);
    return record.subspan(0, record.size() - sizeof(record_id_t));
}

page_id_t BPTree::ReadInnerPageId(Page& page, slot_id_t slot_id)
{
    std::span<const char> record = page.ReadSlot(slot_id);
    page_id_t page_id;
    std::memcpy(&page_id, record.data() + record.size() - sizeof(page_id_t), sizeof(page_id_t));
    return page_id;
}

std::span<const char> BPTree::ReadInnerKey(Page& page, slot_id_t slot_id)
{
    std::span<const char> record = page.ReadSlot(slot_id);
    return record.subspan(0, record.size() - sizeof(page_id_t));
}

/*-----------------------------------------------------------------------------
 * Search Helper Functions
 *-----------------------------------------------------------------------------
 */

/**
 * Finds the appropriate child slot to navigate to in an inner node.
 *
 * In a B+ tree inner node, each slot contains (key || page_id). This function
 * finds which child pointer to follow based on the search key:
 * - Returns the first slot where search_key < slot_key (the pivot)
 * - If all slot keys are <= search_key, returns the last slot (rightmost child)
 *
 * This implements the standard B+ tree routing: keys < pivot go left,
 * keys >= pivot go right.
 *
 * @param inner_page The inner node to search (must be PageType::BPTreeInner)
 * @param key The search key
 * @return slot_id_t The slot containing the appropriate child page_id
 */
slot_id_t BPTree::FindPivotKey(Page& inner_page, std::span<const char> key)
{
    auto slot_iter = std::find_if(inner_page.begin(), inner_page.end(), [&](slot_id_t slot_id){
            auto record_key = ReadInnerKey(inner_page, slot_id);
            return std::lexicographical_compare(key.begin(), key.end(), record_key.begin(), record_key.end());
    });

    // If no pivot found (all keys <= search key), navigate to rightmost child
    return slot_iter != inner_page.end() ? *slot_iter : *std::prev(inner_page.end());
}

/**
 * Finds an exact key match in a leaf node.
 *
 * Searches the leaf page for a slot containing the exact key. Used for
 * lookups and deletions.
 *
 * @param leaf_page The leaf node to search (must be PageType::BPTreeLeaf)
 * @param key The key to search for
 * @return PageIterator Iterator to slot if found, end() otherwise
 */
PageIterator BPTree::FindKey(Page& leaf_page, std::span<const char> key)
{
    auto slot_iter = std::find_if(leaf_page.begin(), leaf_page.end(), [&](slot_id_t slot_id){
            auto record_key = ReadLeafKey(leaf_page, slot_id);
            return std::ranges::equal(record_key, key);
    });
    return slot_iter;
}

/**
 * Finds the position where a key should be inserted in a given node.
 *
 * Returns a PageIterator at the slot where the new key should be inserted
 * to maintain sorted (lexicographic) order in the node. Uses binary search
 * semantics:
 * - Returns the first slot where search_key < slot_key
 * - If all keys are <= search_key, returns the end iterator for the page.
 */
PageIterator BPTree::FindInsertPosition(Page& node, std::span<const char> key)
{
    PageIterator final_slot;
    if (node.GetPageType() == PageType::BPTreeInner) {
        final_slot = node.end();
    } else if (node.GetPageType() == PageType::BPTreeLeaf) {
        /* the last slot in leaf nodes is a sibling pointer NOT a key. Skip
         * this slot. */
        final_slot = std::prev(node.end());
    }

    auto slot_iter = std::find_if(node.begin(), std::prev(node.end()), [&](slot_id_t slot_id){
            auto record_key = ReadLeafKey(node, slot_id);
            return std::ranges::lexicographical_compare(key, record_key);
    });
    return slot_iter != final_slot ? slot_iter : node.end();
}

/*-----------------------------------------------------------------------------
 * Key Insertion Functions
 *-----------------------------------------------------------------------------
 */

/**
 * Templated InsertKey implementation that works for both leaf and inner nodes.
 * Stores the record as (key || value) where value is a fixed-size type.
 */
template<typename ValueType>
void BPTree::InsertKey(Page& page, std::span<const char> key, ValueType value)
{
    static_assert(std::is_trivially_copyable_v<ValueType>,
                  "ValueType must be trivially copyable");

    PageIterator location = FindInsertPosition(page, key);

    // Check if we can insert without reusing deleted slots (maintains sorted order)
    uint16_t value_size = key.size() + sizeof(ValueType);
    if (!page.CanInsert(value_size)) {
        throw std::runtime_error("Insufficient space to insert key");
    }

    // Shift slots to make room (handles both end() and mid-list cases)
    std::optional<slot_id_t> slot = page.ShiftSlotsRight(location);
    if (!slot.has_value()) {
        throw std::runtime_error("Failed to shift slots right");
    }

    /* create a buffer to copy data into. TODO I REALLY don't like this
     * interface at all with the unneeded copy. Going to look into if I can
     * just pass a span from the page directly to write into. Not the safest
     * but I don't see a cleaner solution that gives no copies */
    std::unique_ptr<char[]> record_buffer = std::make_unique<char[]>(value_size);
    std::memcpy(record_buffer.get(), key.data(), key.size());
    std::memcpy(record_buffer.get() + key.size(), &value, sizeof(ValueType));

    page.ResizeSlot(*slot, value_size);
    page.WriteSlot(*slot, { record_buffer.get(), value_size });
}

/* Non-templated wrappers for backward compatibility */
void BPTree::InsertKeyLeaf(Page& page, std::span<const char> key, record_id_t record_id)
{
    InsertKey<record_id_t>(page, key, record_id);
}

void BPTree::InsertKeyInner(Page& page, std::span<const char> key, page_id_t page_id)
{
    InsertKey<page_id_t>(page, key, page_id);
}

// Explicit template instantiations
template void BPTree::InsertKey<record_id_t>(Page& page, std::span<const char> key, record_id_t value);
template void BPTree::InsertKey<page_id_t>(Page& page, std::span<const char> key, page_id_t value);
