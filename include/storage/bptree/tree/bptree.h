#pragma once

#include <shared_mutex>
#include <utility>
#include <optional>
#include <stack>

#include "buffer/page_buffer_manager.h"
#include "page/page.h"
#include "page/page_layout.h"

/**
 * A page with an associated lock held on its frame.
 *
 * This struct bundles a Page with a lock on its underlying Frame to ensure
 * proper RAII semantics when passing locked pages around. The lock is
 * automatically released when the struct is destroyed.
 *
 * IMPORTANT: The lock is on the Frame, not the Page. This allows the Page
 * handle to be moved freely while the Frame remains locked and stable.
 *
 * @tparam LockType Either std::shared_lock<Frame> or std::unique_lock<Frame>
 */
template<typename LockType>
struct LockedPage {
    Page page;
    LockType lock;

    LockedPage(Page&& p, LockType&& l)
        : page(std::move(p)), lock(std::move(l))
    {}

    // Allow moving but not copying (locks are move-only)
    LockedPage(LockedPage&&) = default;
    LockedPage& operator=(LockedPage&&) = default;
    LockedPage(const LockedPage&) = delete;
    LockedPage& operator=(const LockedPage&) = delete;
};

// Convenient type aliases for common lock types
using SharedLockedPage = LockedPage<std::shared_lock<Frame>>;
using ExclusiveLockedPage = LockedPage<std::unique_lock<Frame>>;

// TODO add a comment describing the layout of records in the slotted page for
// how we organize leaf and inner nodes with keys and page/record ids.
// Also need to handle sibling pointers
class BPTree {
public:
    /* Creates an entirely new BPTree */
    BPTree(PageBufferManager* page_buffer_manager);

    /* Get a handle to an existing B+ Tree */
    BPTree(PageBufferManager* page_buffer_manager, page_id_t root_page_id);

    std::optional<record_id_t> Search(std::span<const char> key);
    void Insert(std::span<const char> key, record_id_t);
    void Delete(std::span<const char> key);
private:
    bool InsertOptimistic(std::span<const char> key, record_id_t record_id);
    void InsertPessimistic(std::span<const char> key, record_id_t record_id);

    bool DeleteOptimistic(std::span<const char> key);
    void DeletePessimistic(std::span<const char> key);

    /* Navigate to leaf node using hand-over-hand locking.
     * Returns the leaf page and a lock held on it. */
    SharedLockedPage NavigateToLeafShared(std::span<const char> key);
    ExclusiveLockedPage NavigateToLeafExclusive(std::span<const char> key);

    /* Templated implementation for NavigateToLeaf that works with any lock type.
     * LockType should be std::shared_lock<Page> or std::unique_lock<Page>. */
    template<typename LockType>
    LockedPage<LockType> NavigateToLeafImpl(std::span<const char> key);

    page_id_t SplitNode(std::stack<ExclusiveLockedPage>& tree_traversal);
    PageIterator FindSplitKey(Page& page);

    /* HELPER FUNCTIONS
     * NOTE: these function do not handle locking it is expected that locking
     * is already handled by parent functions */
    record_id_t ReadLeafRecordId(Page& page, slot_id_t slot_id);
    std::span<const char> ReadLeafKey(Page& page, slot_id_t slot_id);

    page_id_t ReadInnerPageId(Page& page, slot_id_t slot_id);
    std::span<const char> ReadInnerKey(Page& page, slot_id_t slot_id);

    /* Finds which child slot to follow in an inner node for the given key.
     * Returns the slot containing the appropriate child page_id. */
    slot_id_t FindPivotKey(Page& inner_page, std::span<const char> key);

    // TODO fix this doc
    /* Finds an exact key match in a leaf node.
     * Returns the slot_id if found, std::nullopt otherwise. */
    PageIterator FindKey(Page& leaf_page, std::span<const char> key);

    /* Finds the position where a key should be inserted into an node.
     * Returns a page iterator in the position where the key should be inserted
     * to maintain sorted order. If the key is greater than all existing keys,
     * returns the end iterator of the page */
    PageIterator FindInsertPosition(Page& node, std::span<const char> key);

    /* Inserts the given key and value into a page (works for both leaf and inner nodes).
     * The value is stored as (key || value) where ValueType must be trivially copyable.
     * This function performs no splitting and simply adds the key locally to the page.
     *
     * PAGE MUST BE ABLE TO INSERT THE KEY AND VALUE! Otherwise, function will throw.
     *
     * @tparam ValueType Must be trivially copyable (e.g., record_id_t, page_id_t)
     * @param page The page to insert into
     * @param key The key to insert
     * @param value The value associated with the key
     */
    template<typename ValueType>
    void InsertKey(Page& page, std::span<const char> key, ValueType value);

    /* Legacy non-templated wrappers for backward compatibility */
    void InsertKeyLeaf(Page& page, std::span<const char> key, record_id_t record_id);
    void InsertKeyInner(Page& page, std::span<const char> key, page_id_t page_id);

    page_id_t CreateNewRoot(std::span<const char> key, page_id_t left_page_id, page_id_t right_page_id);

private:
    page_id_t root_page_id_m;
    PageBufferManager* page_buffer_manager_m;
};
