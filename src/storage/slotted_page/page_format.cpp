#include "storage/page/page_format.h"
#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <format>
#include <queue>
#include <span>
#include <vector>
#include "core/assert.h"
#include "storage/page/checksum.h"
#include "storage/buffer_manager/page.h"

namespace page {

/*-----------------------------------------------------------------------------
   _                    _
  | |__   ___  __ _  __| | ___ _ __
  | '_ \ / _ \/ _` |/ _` |/ _ \ '__|
  | | | |  __/ (_| | (_| |  __/ |
  |_| |_|\___|\__,_|\__,_|\___|_|
-----------------------------------------------------------------------------*/
PageType GetPageType(const Page& page)
{
    PageType page_type;
    memcpy(&page_type, page.GetView().data() + Header::Offsets::PAGE_TYPE, sizeof(page_type));
    return page_type;
}

uint16_t GetNumTuples(const Page& page)
{
    uint16_t num_tuples;
    memcpy(&num_tuples, page.GetView().data() + Header::Offsets::NUM_TUPLES, sizeof(num_tuples));
    return num_tuples;
}

uint64_t GetChecksum(const Page& page)
{
    uint64_t checksum;
    memcpy(&checksum, page.GetView().data() + Header::Offsets::CHECKSUM, sizeof(checksum));
    return checksum;
}

offset_t GetStartFreeSpace(const Page& page)
{
    offset_t start_free_space;
    memcpy(&start_free_space, page.GetView().data() + Header::Offsets::FREE_START, sizeof(start_free_space));
    return start_free_space;
}

offset_t GetEndFreeSpace(const Page& page)
{
    offset_t end_free_space;
    memcpy(&end_free_space, page.GetView().data() + Header::Offsets::FREE_END, sizeof(end_free_space));
    return end_free_space;
}

offset_t GetFreeSpaceSize(const Page& page)
{
    return GetEndFreeSpace(page) - GetStartFreeSpace(page);
}

void SetPageType(const Page& page, PageType page_type)
{
    memcpy(page.GetMutView().data() + Header::Offsets::PAGE_TYPE, &page_type, sizeof(page_type));
}

void SetNumTuples(const Page& page, uint16_t num_tuples)
{
    memcpy(page.GetMutView().data() + Header::Offsets::NUM_TUPLES, &num_tuples, sizeof(num_tuples));
}

void SetChecksum(const Page& page, uint64_t checksum)
{
    memcpy(page.GetMutView().data() + Header::Offsets::CHECKSUM, &checksum, sizeof(checksum));
}

void SetStartFreeSpace(const Page& page, offset_t offset)
{
    memcpy(page.GetMutView().data() + Header::Offsets::FREE_START, &offset, sizeof(offset));
}

void SetEndFreeSpace(const Page& page, offset_t offset)
{
    memcpy(page.GetMutView().data() + Header::Offsets::FREE_END, &offset, sizeof(offset));
}

void InitPage(const Page& page, PageType page_type)
{
    SetPageType(page, page_type);
    SetNumTuples(page, 0);
    SetStartFreeSpace(page, Header::SIZE);
    SetEndFreeSpace(page, PAGE_SIZE);
}

bool ValidChecksum(const Page& page)
{
    return checksum64(page.GetView()) == 0x00;
}

void UpdateChecksum(const Page& page)
{
    SetChecksum(page, 0x00);
    uint64_t new_checksum = checksum64(page.GetView());
    SetChecksum(page, new_checksum);
}


/*-----------------------------------------------------------------------------
       _       _             _
   ___| | ___ | |_ ___ _ __ | |_ _ __ _   _
  / __| |/ _ \| __/ _ \ '_ \| __| '__| | | |
  \__ \ | (_) | ||  __/ | | | |_| |  | |_| |
  |___/_|\___/ \__\___|_| |_|\__|_|   \__, |
                                      |___/
-----------------------------------------------------------------------------*/
uint16_t GetPageCapacity(const Page& page)
{
    return (GetStartFreeSpace(page) - Header::SIZE) / SlotEntry::SIZE;
}

bool IsSlotDeleted(const Page& page, slot_id_t slot_id)
{
    YADB_ASSERT(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity(page),
            std::format("Slot id {} is out of range [0, {}]\n", slot_id, GetSlotDirectoryCapacity(page)).c_str()
            );
    uint8_t deleted;
    offset_t deleted_offset = Header::SIZE + slot_id * SlotEntry::SIZE + SlotEntry::Offsets::DELETED;
    memcpy(&deleted, page.GetView().data() + deleted_offset, sizeof(deleted));
    return deleted > 0;
}

offset_t GetSlotOffset(const Page& page, slot_id_t slot_id)
{
    YADB_ASSERT(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity(page),
            std::format("Slot id {} is out of range [0, {}]\n", slot_id, GetSlotDirectoryCapacity(page)).c_str()
            );
    offset_t offset;
    offset_t slot_entry_offset = Header::SIZE + slot_id * SlotEntry::SIZE + SlotEntry::Offsets::OFFSET;
    memcpy(&offset, page.GetView().data() + slot_entry_offset, sizeof(offset));
    return offset;
}

uint16_t GetSlotSize(const Page& page, slot_id_t slot_id)
{
    YADB_ASSERT(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity(page),
            std::format("Slot id {} is out of range [0, {}]\n", slot_id, GetSlotDirectoryCapacity(page)).c_str()
            );
    uint16_t slot_size;
    offset_t slot_entry_offset = Header::SIZE + slot_id * SlotEntry::SIZE + SlotEntry::Offsets::TUPLE_SIZE;
    memcpy(&slot_size, page.GetView().data() + slot_entry_offset, sizeof(slot_size));
    return slot_size;
}

PageSlice ReadRecord(const Page& page, slot_id_t slot_id)
{
    YADB_ASSERT(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity(page),
            std::format("Slot id {} is out of range [0, {}]\n", slot_id, GetSlotDirectoryCapacity(page)).c_str()
            );
    YADB_ASSERT(!IsSlotDeleted(page, slot_id),
            std::format("Slot {} is deleted", slot_id).c_str()
            );
    return page.GetView().subspan(GetSlotOffset(page, slot_id), GetSlotSize(page, slot_id));
}

MutPageSlice WriteRecord(const Page& page, slot_id_t slot_id)
{
    YADB_ASSERT(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity(page),
            std::format("Slot id {} is out of range [0, {}]\n", slot_id, GetSlotDirectoryCapacity(page)).c_str()
            );
    YADB_ASSERT(!IsSlotDeleted(page, slot_id),
            std::format("Slot {} is deleted", slot_id).c_str()
            );
    return page.GetMutView().subspan(GetSlotOffset(page, slot_id), GetSlotSize(page, slot_id));
}

std::optional<slot_id_t> AllocateSlot(const Page& page, size_t size)
{
    if (SlotEntry::SIZE + size > GetFreeSpaceSize(page))
        return {};

    /* allocating space for the record */
    offset_t record_offset = GetEndFreeSpace(page) - size;
    SetEndFreeSpace(page, record_offset);

    /* Creating the slot directory entry */
    slot_id_t new_slot_id = (GetStartFreeSpace(page) - Header::SIZE) / SlotEntry::SIZE;
    SetStartFreeSpace(page, GetStartFreeSpace(page) + SlotEntry::SIZE);
    SetSlotDeleted(page, new_slot_id, false);
    SetSlotOffset(page, new_slot_id, record_offset);
    SetSlotSize(page, new_slot_id, size);

    /* update number of tuples in page */
    SetNumTuples(page, GetNumTuples(page) + 1);

    return new_slot_id;
}

/* This function will attempt to reuse a deleted slot. If the deleted slot
 * has a previously allocated record that was <= to the size of the newly
 * desired slot the old record allocation will be reused otherwise freespace
 * will be used to allocate new space for the record */
std::optional<slot_id_t> AllocateSlotOrReuseSlot(const Page& page, size_t size)
{
    for (slot_id_t slot = 0; slot < GetPageCapacity(page); slot++) {
        if (!IsSlotDeleted(page, slot))
            continue;

        /* Old record allocation wasn't big enough so we make a new one */
        if (size > GetSlotSize(page, slot)) {
            /* need to have enough free space to allocate the new slot */
            if (size > GetFreeSpaceSize(page))
                continue;
            offset_t record_offset = GetEndFreeSpace(page) - size;
            SetEndFreeSpace(page, record_offset);
            SetSlotOffset(page, slot, record_offset);
        }

        SetSlotDeleted(page, slot, false);
        SetSlotSize(page, slot, size);

        /* update number of tuples in page */
        SetNumTuples(page, GetNumTuples(page) + 1);

        return slot;
    }

    /* if we can't reuse anything just allocate a new slot */
    return AllocateSlot(page, size);
}

void DeleteSlot(const Page& page, slot_id_t slot_id)
{
    SetNumTuples(page, GetNumTuples(page) - 1);
    SetSlotDeleted(page, slot_id, true);
}

void SetSlotDeleted(const Page& page, slot_id_t slot_id, bool deleted)
{
    YADB_ASSERT(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity(page),
            std::format("Slot id {} is out of range [0, {}]\n", slot_id, GetSlotDirectoryCapacity(page)).c_str()
            );

    uint8_t value = deleted ? 1 : 0;
    offset_t slot_deleted_offset = Header::SIZE
        + slot_id * SlotEntry::SIZE
        + SlotEntry::Offsets::DELETED;
    memcpy(page.GetMutView().data() + slot_deleted_offset, &value, sizeof(value));
}

void SetSlotOffset(const Page& page, slot_id_t slot_id, offset_t offset)
{
    YADB_ASSERT(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity(page),
            std::format("Slot id {} is out of range [0, {}]\n", slot_id, GetSlotDirectoryCapacity(page)).c_str()
            );
    offset_t slot_offset_offset = Header::SIZE
        + slot_id * SlotEntry::SIZE
        + SlotEntry::Offsets::OFFSET;
    memcpy(page.GetMutView().data() + slot_offset_offset, &offset, sizeof(offset));
}

void SetSlotSize(const Page& page, slot_id_t slot_id, uint16_t size)
{
    YADB_ASSERT(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity(page),
            std::format("Slot id {} is out of range [0, {}]\n", slot_id, GetSlotDirectoryCapacity(page)).c_str()
            );
    uint16_t slot_size_offset = Header::SIZE
        + slot_id * SlotEntry::SIZE
        + SlotEntry::Offsets::TUPLE_SIZE;
    memcpy(page.GetMutView().data() + slot_size_offset, &size, sizeof(size));
}

/*-----------------------------------------------------------------------------
 _          _                    __                  _   _
| |__   ___| |_ __   ___ _ __   / _|_   _ _ __   ___| |_(_) ___  _ __  ___
| '_ \ / _ \ | '_ \ / _ \ '__| | |_| | | | '_ \ / __| __| |/ _ \| '_ \/ __|
| | | |  __/ | |_) |  __/ |    |  _| |_| | | | | (__| |_| | (_) | | | \__ \
|_| |_|\___|_| .__/ \___|_|    |_|  \__,_|_| |_|\___|\__|_|\___/|_| |_|___/
             |_|
-----------------------------------------------------------------------------*/
void PrintPage(const Page& page)
{
    std::cout << "Page type: " << static_cast<uint8_t>(GetPageType(page)) << '\n';
    std::cout << "Number of slots: " << GetNumTuples(page) << '\n';
    std::cout << "Free space start: " << GetStartFreeSpace(page) << '\n';
    std::cout << "Free space end: " << GetEndFreeSpace(page) << '\n';
    std::cout << "Checksum: " << GetChecksum(page) << "\n\n";

    constexpr std::size_t bytes_per_line = 16;
    for (std::size_t i = 0; i < page.GetView().size(); i += bytes_per_line) {
        // Print offset
        std::cout << std::setw(6) << std::setfill('0') << std::hex << i << "  ";

        // Hex bytes
        for (std::size_t j = 0; j < bytes_per_line; ++j) {
            if (i + j < page.GetView().size()) {
                unsigned byte = static_cast<unsigned char>(page.GetView()[i + j]);
                std::cout << std::setw(2) << byte << ' ';
            } else {
                std::cout << "   "; // padding
            }
        }

        // ASCII view
        std::cout << " |";
        for (std::size_t j = 0; j < bytes_per_line && i + j < page.GetView().size(); ++j) {
            unsigned char c = static_cast<unsigned char>(page.GetView()[i + j]);
            std::cout << (std::isprint(c) ? static_cast<char>(c) : '.');
        }
        std::cout << "|\n";
    }

    // Reset stream flags
    std::cout << std::dec << std::setfill(' ');
}


void VacuumPage(const Page& page)
{
    struct SlotEntry {
        slot_id_t slot_id;
        offset_t offset;
        uint16_t size;
    };

    struct SlotEntryCompare {
        bool operator()(const SlotEntry& l, const SlotEntry& r)
        {
            return l.offset < r.offset;
        }
    };

    std::priority_queue<SlotEntry, std::vector<SlotEntry>, SlotEntryCompare> pq;

    for (slot_id_t id = 0; id < GetPageCapacity(page); id++) {
        SlotEntry slot_entry {
            .slot_id = id,
            .offset = GetSlotOffset(page, id),
            .size = GetSlotSize(page, id),
        };

        if (IsSlotDeleted(page, id)) {
            SetSlotSize(page, id, 0);
        } else {
            pq.push(slot_entry);
        }
    }

    offset_t freespace_end = PAGE_SIZE;
    while (!pq.empty()) {
        SlotEntry slot_entry = pq.top();
        pq.pop();

        freespace_end -= slot_entry.size;
        memmove(page.GetMutView().data() + freespace_end, page.GetMutView().data() + slot_entry.offset, slot_entry.size);
        SetSlotOffset(page, slot_entry.slot_id, freespace_end);
    }
    SetEndFreeSpace(page, freespace_end);
}

}
