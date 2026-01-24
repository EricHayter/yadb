/*-----------------------------------------------------------------------------
 *
 * page_format.h
 *      Slotted page format specification and constants
 *
 * This header defines the low-level layout of pages in the database, including
 * the structure of the page header, slot directory entries, and their offsets.
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
 *  - 64 bit checksum (8 bytes)
 *  - an enum indicating the type of page (1 byte)
 *  - the number of tuples in the page (2 bytes)
 *  - offset to the start of the free space (inclusive) (2 bytes)
 *  - offset to the end of the free space (exclusive) (2 bytes)
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
 *  | Deleted: uint8_t (1 byte)  |
 *  |----------------------------|
 *  | Offset: uint16_t (2 bytes) |
 *  |----------------------------|
 *  | Size: uint16_t (2 bytes)   |
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
#include <optional>
#include <span>
#include <stdexcept>
#include <string>

#include "page.h"
#include "common/definitions.h"

namespace page {

enum class PageType : uint8_t {
    Data = 0x0,
    BPTreeInner = 0x1,
    BPTreeLeaf = 0x2,
};


/*-----------------------------------------------------------------------------
   _                    _
  | |__   ___  __ _  __| | ___ _ __
  | '_ \ / _ \/ _` |/ _` |/ _ \ '__|
  | | | |  __/ (_| | (_| |  __/ |
  |_| |_|\___|\__,_|\__,_|\___|_|
-----------------------------------------------------------------------------*/
namespace Header {

/* offset into the page header for fields */
namespace Offsets {
    constexpr offset_t CHECKSUM = 0x00;
    constexpr offset_t PAGE_TYPE = CHECKSUM + sizeof(uint64_t);
    constexpr offset_t NUM_TUPLES = PAGE_TYPE + sizeof(PageType);
    constexpr offset_t FREE_START = NUM_TUPLES + sizeof(slot_id_t);
    constexpr offset_t FREE_END = FREE_START + sizeof(offset_t);
};

/* size of the page header */
constexpr offset_t SIZE = Offsets::FREE_END + sizeof(offset_t);
};

PageType GetPageType(const Page& page);
uint16_t GetNumTuples(const Page& page);
uint64_t GetChecksum(const Page& page);
offset_t GetStartFreeSpace(const Page& page);
offset_t GetEndFreeSpace(const Page& page);
offset_t GetFreeSpaceSize(const Page& page);

void SetPageType(const Page& page, PageType page_type);
void SetNumTuples(const Page&, uint16_t num_tuples);
void SetChecksum(const Page&, uint64_t checksum);
void SetStartFreeSpace(const Page&, offset_t offset);
void SetEndFreeSpace(const Page&, offset_t offset);

/* initializes the fields inside of the page header. NOTE: This should only
* be used when creating a page for the first time i.e. right after calling
* NewPage with the page buffer manager. */
void InitPage(const Page& page, PageType page_type);

/* Check if the checksum for a page is valid */
bool ValidChecksum(const Page& page);

/* Updates the checksum field in the page header. This MUST be called
* before flushing pages as the checksum will be validated on page
* load */
void UpdateChecksum(const Page& page);


/*-----------------------------------------------------------------------------
       _       _             _
   ___| | ___ | |_ ___ _ __ | |_ _ __ _   _
  / __| |/ _ \| __/ _ \ '_ \| __| '__| | | |
  \__ \ | (_) | ||  __/ | | | |_| |  | |_| |
  |___/_|\___/ \__\___|_| |_|\__|_|   \__, |
                                      |___/
-----------------------------------------------------------------------------*/
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

class ChecksumValidationException : public std::runtime_error {
private:
    page_id_t page_id_;

public:
    ChecksumValidationException(page_id_t page_id, const std::string& msg)
        : std::runtime_error(msg)
        , page_id_(page_id)
    {
    }

    page_id_t page_id() const noexcept { return page_id_; }
};

/* Slot Directory Accessors */
/* returns the number of slot directory entries (including deleted) */
uint16_t GetSlotDirectoryCapacity(const Page& page);
bool IsSlotDeleted(const Page& page, slot_id_t slot_id);
offset_t GetOffset(const Page& page, slot_id_t slot_id);
uint16_t GetSlotSize(const Page& page, slot_id_t slot_id);

PageSlice ReadRecord(const Page& page, slot_id_t slot);
MutPageSlice WriteRecord(const Page& page, slot_id_t slot_id);
std::optional<slot_id_t> AllocateSlot(const Page& page, size_t size);
std::optional<slot_id_t> AllocateSlotOrReuseSlot(const Page& page, size_t size);
void DeleteSlot(const Page& page, slot_id_t slot_id);

/* Slot Directory Mutators */
/* This function just sets the deleted field in the slot entry it DOES NOT
 * update the tuple count in the page header */
void SetSlotDeleted(const Page& page, slot_id_t slot_id, bool deleted);
void SetSlotOffset(const Page& page, slot_id_t slot_id, offset_t offset);
void SetSlotSize(const Page& page, slot_id_t slot_id, uint16_t size);

/*-----------------------------------------------------------------------------
 _          _                    __                  _   _
| |__   ___| |_ __   ___ _ __   / _|_   _ _ __   ___| |_(_) ___  _ __  ___
| '_ \ / _ \ | '_ \ / _ \ '__| | |_| | | | '_ \ / __| __| |/ _ \| '_ \/ __|
| | | |  __/ | |_) |  __/ |    |  _| |_| | | | | (__| |_| | (_) | | | \__ \
|_| |_|\___|_| .__/ \___|_|    |_|  \__,_|_| |_|\___|\__|_|\___/|_| |_|___/
             |_|
-----------------------------------------------------------------------------*/

/* Print out contents of page and values for headers. Mainly to be used
 * for debugging purposes */
void PrintPage(const Page& page);


/* Reacquires freed space from deleted tuples. This is a fairly expensive
 * operation use it sparingly. */
void VacuumPage(const Page& page);

}
