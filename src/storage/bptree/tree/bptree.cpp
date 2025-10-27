#include "tree/bptree.h"
#include "page/page_iterator.h"
#include "page/page_layout.h"

#include <cassert>

#include <algorithm>
#include <shared_mutex>

BPTree::BPTree(PageBufferManager* page_buffer_manager)
    : page_buffer_manager_m(page_buffer_manager)
{
    assert(page_buffer_manager_m);

    root_page_id_m = page_buffer_manager_m->AllocatePage();
    Page page = page_buffer_manager_m->GetPage(root_page_id_m);
    std::lock_guard<Page> lg(page);
    page.InitPage(PageType::BPTreeLeaf);
}

BPTree::BPTree(PageBufferManager* page_buffer_manager, page_id_t root_page_id)
    : page_buffer_manager_m(page_buffer_manager)
    , root_page_id_m(root_page_id)
{
    assert(page_buffer_manager_m);
}


std::optional<record_id_t> BPTree::Search(std::span<const char> key)
{
    page_id_t current_page_id = root_page_id_m;

    // Start with root page
    Page current_page = page_buffer_manager_m->GetPage(current_page_id);
    std::shared_lock<Page> current_lock(current_page);

    while (true) {
        PageType page_type = current_page.GetPageType();

        if (page_type == PageType::BPTreeLeaf) {
            // We're at a leaf - search for the key
            // Leaf format: (key || record_id) pairs, last slot is sibling pointer
            auto it = std::find_if(
                current_page.begin(),
                std::prev(current_page.end()), // Skip sibling pointer
                [&](const auto& slot_value) {
                    auto [_, record] = slot_value;
                    // Extract key portion (everything except record_id at end)
                    auto current_key = record.subspan(0, record.size() - sizeof(record_id_t));
                    return std::ranges::equal(current_key, key);
                }
            );

            if (it == std::prev(current_page.end())) {
                return std::nullopt; // Key not found
            }

            // Extract record_id from the end of the record
            auto [_, record] = *it;
            record_id_t record_id;
            std::memcpy(&record_id,
                       record.data() + record.size() - sizeof(record_id_t),
                       sizeof(record_id_t));
            return record_id;
        }
        else if (page_type == PageType::BPTreeInner) {
            // Inner node format: (key || child_page_id) pairs
            // Find the appropriate child pointer

            page_id_t next_page_id;

            // Find first key > search key, then take that pointer
            // or take the last pointer if all keys <= search key
            auto it = std::find_if(
                current_page.begin(),
                current_page.end(),
                [&](const auto& slot_value) {
                    auto [_, record] = slot_value;
                    auto current_key = record.subspan(0, record.size() - sizeof(page_id_t));
                    // Return true if current_key > key (found the boundary)
                    return std::lexicographical_compare(
                        key.begin(), key.end(),
                        current_key.begin(), current_key.end()
                    );
                }
            );

            // Extract child page_id from the record
            auto [_, record] = (it != current_page.end()) ? *it : *std::prev(current_page.end());
            std::memcpy(&next_page_id,
                       record.data() + record.size() - sizeof(page_id_t),
                       sizeof(page_id_t));

            // HAND-OVER-HAND: Get and lock child BEFORE releasing parent
            Page child_page = page_buffer_manager_m->GetPage(next_page_id);
            std::shared_lock<Page> child_lock(child_page);

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

void BPTree::Insert(std::span<const char> key, record_id_t record_id)
{
    if (!InsertOptimistic(key, record_id))
        InsertPessimistic(key, record_id);
}

bool BPTree::InsertOptimistic(std::span<const char> key, record_id_t record_id)
{
    return false;
}
