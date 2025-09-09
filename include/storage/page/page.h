#pragma once

#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>

using page_id_t = uint32_t;
using slot_id_t = uint16_t; // slot id inside of slot directory
using offset_t = uint16_t; // offset into slotted page.

// view/span into a page-sized buffer. Immutable by default so I don't blow my
// leg off by accidentally handling a mutable view.
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
namespace Offsets {
    constexpr offset_t PAGE_TYPE = 0x00;
    constexpr offset_t NUM_SLOTS = PAGE_TYPE + sizeof(PageType);
    constexpr offset_t FREE_START = NUM_SLOTS + sizeof(slot_id_t);
    constexpr offset_t FREE_END = FREE_START + sizeof(offset_t);
    constexpr offset_t CHECKSUM = FREE_END + sizeof(offset_t);
};
constexpr offset_t SIZE = Offsets::CHECKSUM + sizeof(uint64_t);
};

namespace SlotEntry {
namespace Offsets {
    constexpr offset_t DELETED = 0x00;
    constexpr offset_t OFFSET = DELETED + sizeof(uint8_t);
    constexpr offset_t TUPLE_SIZE = OFFSET + sizeof(offset_t);
};
constexpr offset_t SIZE = Offsets::TUPLE_SIZE + sizeof(uint16_t);

// The stale span in the buffer after logically deleting has been reclaimed by
// another slot.
constexpr offset_t RECLAIMED = std::numeric_limits<offset_t>::max();
};

/**
 * PAGE FORMAT
 * pages in YADB will have a slotted page format:
 *
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
 * TODO figure out how much data I should give to each field in the header
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
 *  The slot directory contains a layer of indirection to each of the tuples in
 *  the page. The slot directory will contain an array of TupleNodes (TODO
 *  find proper name for this) laid out contiguously in memory. Each tuple node
 *  will contain 4 bytes of information:
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
 *  length and offset found in the slot directory entry. This will contain
 *  the actual information of the entry. TODO describe later how tuples will
 *  be laid out in memory and the techniques used for variable length data
 *  (likely going to use some pascal string type of thing).
 */

class PageBufferManager;

class Page {
public:
    Page(PageBufferManager* buffer_manager, page_id_t page_id, PageView page_view, std::shared_lock<std::shared_mutex>&& lk);
    ~Page();
    page_id_t GetPageId() const { return page_id_m; }
    PageType GetPageType() const;
    uint16_t GetNumSlots() const;
    offset_t GetStartFreeSpace() const;
    offset_t GetEndFreeSpace() const;
    uint64_t GetChecksum() const;

    PageView ReadPage() { return page_data_m; }
    std::span<const char> ReadSlot(slot_id_t slot);

protected:
    Page(PageBufferManager* buffer_manager, page_id_t page_id);
    bool IsSlotDeleted(slot_id_t slot_id) const;
    offset_t GetOffset(slot_id_t slot_id) const;
    uint16_t GetSlotSize(slot_id_t slot_id) const;

protected:
    page_id_t page_id_m;
    PageBufferManager* buffer_manager_m;

private:
    std::shared_lock<std::shared_mutex> lk_m;
    PageView page_data_m;
};

class PageMut : public Page {
public:
    PageMut(PageBufferManager* buffer_manager, page_id_t page_id, MutPageView page_view, std::unique_lock<std::shared_mutex>&& lk);
    ~PageMut();
    void InitPage();
    std::optional<slot_id_t> AllocateSlot(uint16_t size);
    void WriteSlot(slot_id_t slot_id, std::span<const char> data);
    void DeleteSlot(slot_id_t slot_id);
    void VacuumPage();

private:
    void SetNumSlots(uint16_t num_slots);
    void SetStartFreeSpace(offset_t offset);
    void SetEndFreeSpace(offset_t offset);
    void SetSlotOffset(slot_id_t slot_id, offset_t offset);
    void SetSlotSize(slot_id_t slot_id, uint16_t size);
    void SetSlotDeleted(slot_id_t slot_id, bool deleted);

private:
    std::unique_lock<std::shared_mutex> lk_m;
    MutPageView page_data_m;
};
