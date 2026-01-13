/*-----------------------------------------------------------------------------
 *
 * page.h
 *      Slotted page interface
 *
 * This header specifies the interface for pages which support
 * the basic operations of inserting and deleting into pages using a slotted
 * page format.
 *
 * As the pages themselves act as a layer of abstraction over page views,
 * the class supports thread-safe reads and writes through the buffer pool.
 *
 *-----------------------------------------------------------------------------
 */

#pragma once

#include <optional>
#include <span>

#include "buffer/frame.h"
#include "page/page_layout.h"
#include "page/page_iterator.h"

class PageBufferManager;
class Frame;
class PageIterator;

/* Page definition with all the basic read-only operations on pages. Such as
 * reading page header fields, reading the slot directory, and validating
 * checksum data. This class is intended to be built on top of for other types
 * of pages using the slotted page format e.g. B+ tree index nodes. */
class Page {
public:
    Page() = default;
    Page(PageBufferManager* page_buffer_manager, Frame* frame);
    Page(Page&& other);
    Page& operator=(Page&& other);
    Page(const Page& other) = delete;
    Page& operator=(const Page& other) = delete;
    ~Page();

    /* Direct frame access for locking */
    Frame* GetFrame() const { return frame_m; }

    /* Page Metadata Operations */
    page_id_t GetPageId() const { return frame_m->page_id; };
    PageType GetPageType() const;
    /* validates the page checksum returns true if valid otherwise false */
    bool ValidChecksum() const;
    /* Updates the checksum field in the page header. This MUST be called
     * before flushing pages as the checksum will be validated on page
     * load */
    void UpdateChecksum();
    /* Print out contents of page and values for headers. Mainly to be used
     * for debugging purposes */
    void PrintPage() const;

    /* Page Initialization */
    /* initializes the fields inside of the page header. NOTE: This should only
     * be used when creating a page for the first time i.e. right after calling
     * NewPage with the page buffer manager. */
    void InitPage(PageType page_type);

    /* Slot Directory Accessors */
    /* Slot Operations */
    /* returns the number of valid slots (i.e. not deleted) */
    uint16_t GetNumSlots() const;

    /* Returns a span to the given tuple in the slotted page. The size of the
     * span will be determined by the size indicated in the slot entry for
     * the tuple */
    std::span<const char> ReadSlot(slot_id_t slot);

    /* returns the number of slot directory entries (including deleted) */
    uint16_t GetSlotDirectoryCapacity() const;

    /* check to see if tuple has been deleted from the page */
    bool IsSlotDeleted(slot_id_t slot_id) const;

    /* returns the length of a tuple */
    uint16_t GetSlotSize(slot_id_t slot_id) const;

    /* returns the slot of a newly allocated tuple of size in the page. If no
     * space can be found for the tuple then returns std::nullopt. */
    std::optional<slot_id_t> AllocateSlot(uint16_t size);
    /* Appends a new slot to the end of the slot directory without trying to
     * reuse deleted entries. This is useful for ordered structures like B+ tree
     * nodes where slot position matters. Returns the slot_id of the appended
     * slot, or std::nullopt if there is insufficient space. */
    std::optional<slot_id_t> AppendSlot(uint16_t size);
    /* Shifts slot directory entries to the right starting from the position.
     * If position is end(), appends 'count' new slots at the end.
     * Otherwise, moves entries at positions [*position, slot_count) to
     * [*position + count, slot_count + count), creating 'count' empty slots.
     * Only the slot directory entries are moved, not the actual tuple data.
     * Returns the slot_id of the first newly created slot, or std::nullopt if
     * there is insufficient space for the additional slot directory entries. */
    std::optional<slot_id_t> ShiftSlotsRight(PageIterator position, uint16_t count = 1);
    void WriteSlot(slot_id_t slot_id, std::span<const char> data);

    /* Updates the size of the buffer of a given slot. If the slot is marked as
     * deleted, this function will activate it (mark as not deleted and increment
     * slot count). WARNING: this function may invalidate the data contained in
     * the buffer originally. Returns true if successful, false if there is
     * insufficient space. */
    bool ResizeSlot(slot_id_t slot_id, uint16_t size);

    /* logically deleted tuple from page. Note this does not immediately free
     * up any space in general. To fully get back space from deletes the
     * page must be vacuumed. */
    void DeleteSlot(slot_id_t slot_id);

    /* Free Space Management */
    offset_t GetFreeSpaceSize() const;
    /* Checks if a record of the given size can be inserted into the page.
     * Returns true if there is sufficient space, false otherwise.
     * If reuse_deleted is true, considers reusing deleted slots; otherwise,
     * only checks if space exists for a new slot directory entry and data. */
    bool CanInsert(uint16_t size, bool reuse_deleted = false) const;
    /* Reacquires freed space from deleted tuples. This is a fairly expensive
     * operation use it sparingly. */
    void VacuumPage();

    /* Iterator Support */
    /* Returns an iterator to the first non-deleted slot */
    PageIterator begin() const;
    /* Returns an iterator to one past the last slot */
    PageIterator end() const;

private:
    /* Page Header Accessors */
    /* returns the value of the checksum field from the page header */
    uint64_t GetChecksum() const;
    /* returns offset to the start of the free space (inclusive) */
    offset_t GetStartFreeSpace() const;
    /* returns the offset to the end of the free space (exclusive) */
    offset_t GetEndFreeSpace() const;

    /* Page Header Mutators */
    void SetNumSlots(uint16_t num_slots);
    void SetStartFreeSpace(offset_t offset);
    void SetEndFreeSpace(offset_t offset);
    void SetChecksum(uint64_t checksum);
    void SetPageType(PageType page_type);

    /* Slot Directory Accessors */
    /* find the offset to the start of a tuple (inclusive) in the page */
    offset_t GetOffset(slot_id_t slot_id) const;

    /* Slot Directory Mutators */
    void SetSlotDeleted(slot_id_t slot_id, bool deleted);
    void SetSlotOffset(slot_id_t slot_id, offset_t offset);
    void SetSlotSize(slot_id_t slot_id, uint16_t size);

    PageBufferManager* page_buffer_manager_m;
    Frame* frame_m;
};
