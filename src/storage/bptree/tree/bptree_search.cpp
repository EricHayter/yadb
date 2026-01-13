/*-----------------------------------------------------------------------------
 *
 * bptree_search.cpp
 *      B+ Tree search operations
 *
 * This file contains the search/lookup functionality for the B+ tree.
 *
 *-----------------------------------------------------------------------------
 */

#include "tree/bptree.h"

#include <optional>

std::optional<record_id_t> BPTree::Search(std::span<const char> key)
{
    auto locked_page = NavigateToLeafShared(key);

    auto slot = FindKey(locked_page.page, key);
    if (slot == locked_page.page.end()) {
        return std::nullopt;
    }

    return ReadLeafRecordId(locked_page.page, *slot);
}
