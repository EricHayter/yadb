#pragma once

#include <span>
#include <optional>
#include <cstdint>

using slot_id_t = uint32_t; // slot id inside of slot directory
using offset_t = uint32_t; // offset into slotted page.

// view/span into a page-sized buffer. Immutable by default so I don't blow my
// leg off by accidentally handling a mutable view.
constexpr std::size_t PAGE_SIZE = 4096;
using MutPageView = std::span<char, PAGE_SIZE>;
using PageView = std::span<const char, PAGE_SIZE>;

enum class PageType {
    Data,
    BPTreeInner,
    BPTreeLeaf
};

/**
 * PAGE FORMAT
 * pages in YADB will have a slotted page format:
 *
 * -------------------
 * | Header          |
 * ------------------|
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
 *  - a bitfield for the type of page
 *      - data file
 *      - index leaf node
 *      - index root node
 *      - index internal node
 *  - the number of tuples in the page
 *  - offset to the start of the free space (inclusive)
 *  - offset to the end of the free space (exclusive)
 *  - 64 bit checksum
 *
 *  2. Slot directory:
 *  The slot directory contains a layer of indirection to each of the tuples in
 *  the page. The slot directory will contain an array of TupleNodes (TODO
 *  find proper name for this) laid out contiguously in memory. Each tuple node
 *  will contain 8 bytes of information:
 *  - length (4 bytes): the size of the tuple being pointed to
 *  - offset (4 bytes): offset in the page to the start of the tuple
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


class ReadPage {
public:
    std::size_t GetNumSlots() const;
    std::size_t GetStartFreeSpace() const;
    std::size_t GetEndFreeSpace() const;

    std::span<const std::byte> ReadSlot(slot_id_t slot);

protected:
    offset_t GetOffset(slot_id_t id) const;
    std::size_t GetSize(slot_id_t id) const;
protected:
    MutPageView data;
};

class WritePage : public ReadPage {
    std::optional<slot_id_t> AllocateSlot(std::size_t size);
    void WriteSlot(slot_id_t slot_id, std::span<const std::byte> data);
    void DeleteSlot(slot_id_t slot_id);
    void CompressPage();
};
