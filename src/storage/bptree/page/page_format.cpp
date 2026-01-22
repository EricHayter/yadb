#include "page/page_format.h"

#include <cstring>
#include <format>
#include <iomanip>
#include <iostream>
#include <queue>

#include "core/assert.h"
#include "page/checksum.h"
#include "page/page.h"

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

uint16_t GetNumSlots(const Page& page)
{
    uint16_t num_slots;
    memcpy(&num_slots, page.GetView().data() + Header::Offsets::NUM_SLOTS, sizeof(num_slots));
    return num_slots;
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

void SetNumSlots(const Page& page, uint16_t num_slots)
{
    memcpy(page.GetMutView().data() + Header::Offsets::NUM_SLOTS, &num_slots, sizeof(num_slots));
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
    SetNumSlots(page, 0);
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
uint16_t GetSlotDirectoryCapacity(const Page& page)
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

offset_t GetOffset(const Page& page, slot_id_t slot_id)
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

std::span<const char> ReadRecord(const Page& page, slot_id_t slot_id)
{
    YADB_ASSERT(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity(page),
            std::format("Slot id {} is out of range [0, {}]\n", slot_id, GetSlotDirectoryCapacity(page)).c_str()
            );
    YADB_ASSERT(!IsSlotDeleted(page, slot_id),
            std::format("Slot {} is deleted", slot_id).c_str()
            );
    return page.GetView().subspan(GetOffset(page, slot_id), GetSlotSize(page, slot_id));
}

std::span<char> WriteRecord(const Page& page, slot_id_t slot_id)
{
    YADB_ASSERT(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity(page),
            std::format("Slot id {} is out of range [0, {}]\n", slot_id, GetSlotDirectoryCapacity(page)).c_str()
            );
    YADB_ASSERT(!IsSlotDeleted(page, slot_id),
            std::format("Slot {} is deleted", slot_id).c_str()
            );
    return page.GetMutView().subspan(GetOffset(page, slot_id), GetSlotSize(page, slot_id));
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
    std::cout << "Number of slots: " << GetNumSlots(page) << '\n';
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

    for (slot_id_t id = 0; id < GetSlotDirectoryCapacity(page); id++) {
        SlotEntry slot_entry {
            .slot_id = id,
            .offset = GetOffset(page, id),
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
