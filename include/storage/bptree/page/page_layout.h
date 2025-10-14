/*-----------------------------------------------------------------------------
 *
 * page_layout.h
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
#include <span>
#include <stdexcept>
#include <string>

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
    constexpr offset_t NUM_SLOTS = PAGE_TYPE + sizeof(PageType);
    constexpr offset_t FREE_START = NUM_SLOTS + sizeof(slot_id_t);
    constexpr offset_t FREE_END = FREE_START + sizeof(offset_t);
};

/* size of the page header */
constexpr offset_t SIZE = Offsets::FREE_END + sizeof(offset_t);
};

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
