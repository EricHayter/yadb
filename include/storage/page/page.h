/*-----------------------------------------------------------------------------
 *
 * page.h
 *      Slotted page interface over a page span
 *
 * This header specifies the interface for variants of pages which will support
 * the basic operations of inserting and deleting into pages using a slotted
 * page.
 *
 * As the page themselves act as a layer of abstraction over page views
 * additional members have been included into the class to support thread
 * safe reads and writes.
 *
 *
 * The page format will approximately be:
 * -------------------
 * | Header          |
 * |-----------------|
 * | Slot Directory  |
 * |-----------------|
 * | | | | | | | | | |
 * | v v v v v v v v |
 * |                 |
 * | Free Space      |
 * |                 |
 * | ^ ^ ^ ^ ^ ^ ^ ^ |
 * | | | | | | | | | |
 * |-----------------|
 * | Tuples          |
 * -------------------
 *
 * The page consists of 4 sections:
 *  1. Header:
 *  the page header will contain important metadata for each page. In specific
 *  it will contain the following fields:
 *  - an enum indicating the type of page (1 byte)
 *  - the number of tuples in the page (2 bytes)
 *  - offset to the start of the free space (inclusive) (2 bytes)
 *  - offset to the end of the free space (exclusive) (2 bytes)
 *  - 64 bit checksum (8 bytes)
 *  total size:
 *  15 bytes
 *
 *  2. Slot directory:
 *  The slot directory is a layer of indirection to each of the tuples in
 *  the page. This greatly simplifies cases where data is shifted e.g. in the
 *  case of vacuuming a page.
 *
 *  The slot directory will contain an array of slot entries laid out
 *  contiguously in memory. Each entry will contain the following information:
 *  - In-Use (1 byte): is the tuple that this slot entry points to deleted?
 *  - Offset (2 bytes): offset in the page to the start of the tuple
 *  - Size (2 bytes): the size of the tuple being pointed to
 *
 *  Slot Entry:
 *  ------------------------------
 *  | Deleted: uint8_t (1 byte)   |
 *  |----------------------------|
 *  | Offset: uint16_t (2 bytes) |
 *  |----------------------------|
 *  | Size: uint16_t (2 bytes) |
 *  ------------------------------
 *
 *  The slot directory will grow "downwards" in the page increasing in offset
 *  as the number of entries grows.
 *
 *  3. Free Space
 *  space unused yet in the page
 *
 *  4. Tuples
 *  For each entry in the slot directory there will be a tuple of the given
 *  length and offset found towards the end of the page. This will contain
 *  the actual information of the entry.
 *
 *-----------------------------------------------------------------------------
 */

#pragma once

#include <cstdint>

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>

using page_id_t = uint32_t;

/* index of a tuple in the slot directory */
using slot_id_t = uint16_t;

/* offset into page */
using offset_t = uint16_t;

/* Size of ALL pages in the database. Maximum allowable value of 65536 due
 * to the constraints on definitions of offset and slot_id types */
constexpr std::size_t PAGE_SIZE = 4096;

using MutPageView = std::span<char, PAGE_SIZE>;
using PageView = std::span<const char, PAGE_SIZE>;

enum class PageType : uint8_t {
    Data = 0x0,
    BPTreeRoot = 0x1,
    BPTreeInner = 0x2,
    BPTreeLeaf = 0x3,
};

namespace Header {
/* offset into the page header for fields */
namespace Offsets {
    constexpr offset_t PAGE_TYPE = 0x00;
    constexpr offset_t NUM_SLOTS = PAGE_TYPE + sizeof(PageType);
    constexpr offset_t FREE_START = NUM_SLOTS + sizeof(slot_id_t);
    constexpr offset_t FREE_END = FREE_START + sizeof(offset_t);
    constexpr offset_t CHECKSUM = FREE_END + sizeof(offset_t);
};
/* size of the page header */
constexpr offset_t SIZE = Offsets::CHECKSUM + sizeof(uint64_t);
};

namespace SlotEntry {
/* offset into the slot entry for fields */
namespace Offsets {
    constexpr offset_t DELETED = 0x00;
    constexpr offset_t OFFSET = DELETED + sizeof(uint8_t);
    constexpr offset_t TUPLE_SIZE = OFFSET + sizeof(offset_t);
};
/* size of slot entries */
constexpr offset_t SIZE = Offsets::TUPLE_SIZE + sizeof(uint16_t);
};

class PageBufferManager;

/* Page definition with all the basic read-only operations on pages. Such as
 * reading page header fields, reading the slot directory, and validating
 * checksum data. This class is intended to be built on top of for other types
 * of pages using the slotted page format e.g. B+ tree index nodes. */
class Page {
public:
    Page(PageBufferManager* buffer_manager, page_id_t page_id, MutPageView page_view, std::shared_lock<std::shared_mutex>&& lk);
    ~Page();
    page_id_t GetPageId() const { return page_id_m; }

    /* validates the page checksum returns true if valid otherwise false */
    bool ValidChecksum() const;

    PageType GetPageType() const;
    uint16_t GetNumSlots() const;
    offset_t GetFreeSpaceSize() const;

    /* Get a read only span for ALL of the page's data. This should be used
     * sparingly if ever used at all
     */
    PageView ReadPage() { return page_data_m; }

    /* Returns a span to the given tuple in the slotted page. The size of the
     * span will be determined by the size indicated in the slot entry for
     * the tuple
     */
    std::span<const char> ReadSlot(slot_id_t slot);

protected:
    /* constructor for subclasses of page */
    Page(PageBufferManager* buffer_manager, page_id_t page_id, MutPageView page_view);

    /* page header related functions */
    /* returns the value of the checksum field from the page header */
    uint64_t GetChecksum() const;

    /* returns offset to the start of the free space (inclusive) */
    offset_t GetStartFreeSpace() const;

    /* returns the offset to the end of the free space (exclusive) */
    offset_t GetEndFreeSpace() const;

    /* slot directory access functions */
    /* check to see if tuple has been deleted from the page */
    bool IsSlotDeleted(slot_id_t slot_id) const;

    /* find the offset to the start of a tuple (inclusive) in the page */
    offset_t GetOffset(slot_id_t slot_id) const;

    /* returns the length of a tuple */
    uint16_t GetSlotSize(slot_id_t slot_id) const;

protected:
    MutPageView page_data_m;

private:
    page_id_t page_id_m;

    /* pointer back to the page buffer manager so that it may be notified
     * when a page is destructed */
    PageBufferManager* buffer_manager_m;

    /* lock for concurrent read access to page_data_m */
    std::shared_lock<std::shared_mutex> data_lk_m;
};

/* Page handle for write access. Has all of the same functionality but also
 * support for updating page headers, slot directory entries, and writing data. */
class PageMut : public Page {
public:
    PageMut(PageBufferManager* buffer_manager, page_id_t page_id, MutPageView page_view, std::unique_lock<std::shared_mutex>&& lk);
    ~PageMut();

    /* initializes the fields inside of the page header. NOTE: This should only
     * be used when creating a page for the first time i.e. right after calling
     * NewPage with the page buffer manager. */
    void InitPage();

    /* Updates the checksum field in the page header. This MUST be called
     * before flushing pages as the checksum will be validated on page
     * load */
    void UpdateChecksum();

    /* returns the slot of a newly allocated tuple of size in the page. If no
     * space can be found for the tuple then returns std::nullopt. */
    std::optional<slot_id_t> AllocateSlot(uint16_t size);

    void WriteSlot(slot_id_t slot_id, std::span<const char> data);

    /* logically deleted tuple from page. Note this does not immediately free
     * up any space in general. To fully get back space from deletes the
     * page must be vacuumed. */
    void DeleteSlot(slot_id_t slot_id);

    /* Reacquires freed space from deleted tuples. This is a fairly expensive
     * operation use it sparingly. */
    void VacuumPage();

private:
    /* functions for updating page header fields */
    void SetNumSlots(uint16_t num_slots);
    void SetStartFreeSpace(offset_t offset);
    void SetEndFreeSpace(offset_t offset);

    /* functions for updating slot directory entries */
    void SetSlotDeleted(slot_id_t slot_id, bool deleted);
    void SetSlotOffset(slot_id_t slot_id, offset_t offset);
    void SetSlotSize(slot_id_t slot_id, uint16_t size);

private:
    /* lock for ownership of page data in Page parent class */
    std::unique_lock<std::shared_mutex> data_lk_m;
};
