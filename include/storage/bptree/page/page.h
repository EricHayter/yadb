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

#include "page/page_layout.h"
#include "buffer/frame.h"

class Frame;

/* Page definition with all the basic read-only operations on pages. Such as
 * reading page header fields, reading the slot directory, and validating
 * checksum data. This class is intended to be built on top of for other types
 * of pages using the slotted page format e.g. B+ tree index nodes. */
class Page {
public:
    /* Constructors and Assignment */
    /* constructor for subclasses of base page */
    Page(Frame* frame);
    /* Allow for transfer of ownership of pages */
    Page(Page&& other);
    Page& operator=(Page&& other);
    /* due to the calling of the AddAccessor and RemoveAccessor there must only
     * be a single owner to a given page i.e. no copies as this would result
     * in duplicate notifications to the page buffer manager. */
    Page(const Page& other) = delete;
    Page& operator=(const Page& other) = delete;
    ~Page();

    /* Locking and unlocking operations */
    void lock();
    bool try_lock();
    void unlock();
    void lock_shared();
    bool try_lock_shared();
    void unlock_shared();

    /* Page Metadata Operations */
    page_id_t GetPageId() const;
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
    void InitPage();

    /* Slot Operations */
    /* returns the number of valid slots (i.e. not deleted) */
    uint16_t GetNumSlots() const;
    /* Returns a span to the given tuple in the slotted page. The size of the
     * span will be determined by the size indicated in the slot entry for
     * the tuple */
    std::span<const char> ReadSlot(slot_id_t slot);
    /* returns the slot of a newly allocated tuple of size in the page. If no
     * space can be found for the tuple then returns std::nullopt. */
    std::optional<slot_id_t> AllocateSlot(uint16_t size);
    void WriteSlot(slot_id_t slot_id, std::span<const char> data);
    /* logically deleted tuple from page. Note this does not immediately free
     * up any space in general. To fully get back space from deletes the
     * page must be vacuumed. */
    void DeleteSlot(slot_id_t slot_id);

    /* Free Space Management */
    offset_t GetFreeSpaceSize() const;
    /* Reacquires freed space from deleted tuples. This is a fairly expensive
     * operation use it sparingly. */
    void VacuumPage();

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

    /* Slot Directory Accessors */
    /* returns the number of slot directory entries (including deleted) */
    uint16_t GetSlotDirectoryCapacity() const;
    /* check to see if tuple has been deleted from the page */
    bool IsSlotDeleted(slot_id_t slot_id) const;
    /* find the offset to the start of a tuple (inclusive) in the page */
    offset_t GetOffset(slot_id_t slot_id) const;
    /* returns the length of a tuple */
    uint16_t GetSlotSize(slot_id_t slot_id) const;

    /* Slot Directory Mutators */
    void SetSlotDeleted(slot_id_t slot_id, bool deleted);
    void SetSlotOffset(slot_id_t slot_id, offset_t offset);
    void SetSlotSize(slot_id_t slot_id, uint16_t size);

    Frame* frame_m;
};
