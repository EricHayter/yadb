#include "storage/page/page.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <queue>

PageType ReadPage::GetPageType() const
{
    PageType page_type;
    memcpy(&page_type, page_data_m.data() + Header::Offsets::PAGE_TYPE, sizeof(PageType));
    return page_type;
}

uint16_t ReadPage::GetNumSlots() const
{
    uint16_t num_slots;
    memcpy(&num_slots, page_data_m.data() + Header::Offsets::NUM_SLOTS, sizeof(uint16_t));
    return num_slots;
}

offset_t ReadPage::GetStartFreeSpace() const
{
    offset_t start_free_space;
    memcpy(&start_free_space, page_data_m.data() + Header::Offsets::FREE_START, sizeof(offset_t));
    return start_free_space;
}

offset_t ReadPage::GetEndFreeSpace() const
{
    offset_t end_free_space;
    memcpy(&end_free_space, page_data_m.data() + Header::Offsets::FREE_END, sizeof(offset_t));
    return end_free_space;
}

uint64_t ReadPage::GetChecksum() const
{
    uint64_t checksum;
    memcpy(&checksum, page_data_m.data() + Header::Offsets::CHECKSUM, sizeof(uint64_t));
    return checksum;
}

std::span<const std::byte> ReadPage::ReadSlot(slot_id_t slot_id)
{
    return std::span<const std::byte>(page_data_m.data() + GetOffset(slot_id), GetSlotSize(slot_id));
}

bool ReadPage::IsSlotDeleted(slot_id_t slot_id) const
{
    uint8_t deleted;
    offset_t deleted_offset = Header::SIZE + slot_id * SlotEntry::SIZE + SlotEntry::Offsets::DELETED;
    memcpy(&deleted, page_data_m.data() + deleted_offset, sizeof(deleted));
    return deleted > 0;
}

offset_t ReadPage::GetOffset(slot_id_t id) const
{
    assert(id >= 0 && id < GetNumSlots());
    offset_t offset;
    offset_t slot_entry_offset = Header::SIZE + id * SlotEntry::SIZE + SlotEntry::Offsets::OFFSET;
    memcpy(&offset, page_data_m.data() + slot_entry_offset, sizeof(offset));
    return offset;
}

uint16_t ReadPage::GetSlotSize(slot_id_t slot_id) const
{
    assert(slot_id >= 0 && slot_id < GetNumSlots());
    uint16_t slot_size;
    offset_t slot_entry_offset = Header::SIZE + slot_id * SlotEntry::SIZE + SlotEntry::Offsets::TUPLE_SIZE;
    memcpy(&slot_size, page_data_m.data() + slot_entry_offset, sizeof(slot_size));
    return slot_size;
}

//// simplest implementation (wastes space by not reusing old slots). Implement other way soon
// std::optional<slot_id_t> WritePage::AllocateSlot(uint16_t size)
//{
//     // need to have space for slot directory entry AND the tuple
//     if (SlotEntry::SIZE + size > GetEndFreeSpace() - GetStartFreeSpace())
//         return std::nullopt;
//
//     offset_t tuple_offset = GetEndFreeSpace() - size;
//
//     // Write header fields
//     uint16_t old_slot_count = GetNumSlots();
//     SetNumSlots(old_slot_count + 1);
//     SetStartFreeSpace(GetStartFreeSpace() + SlotEntry::SIZE);
//     SetEndFreeSpace(tuple_offset);
//
//     // Write slot directory entry
//     SetSlotOffset(old_slot_count, tuple_offset);
//     SetSlotSize(old_slot_count, size);
//
//     return old_slot_count;
// }

// TODO this isn't right. It's trying to reuse the actual slot entry IFF I can reuse the space in the freespace
// I should be able to reuse the slot regardless...
// Technically I guess it's not incorrect but I think it's misleading...
// Space saving implementation
std::optional<slot_id_t> WritePage::AllocateSlot(uint16_t size)
{
    // try and reuse a deleted entry first
    for (slot_id_t slot_id = 0; slot_id < GetNumSlots(); slot_id++) {
        if (IsSlotDeleted(slot_id) && GetOffset(slot_id) != SlotEntry::RECLAIMED && GetSlotSize(slot_id) <= size) {
            SetSlotSize(slot_id, size);
            SetSlotDeleted(slot_id, false);
        }
    }

    // Can't reuse any deleted slots.
    // need to have space for slot directory entry AND the tuple
    if (SlotEntry::SIZE + size > GetEndFreeSpace() - GetStartFreeSpace())
        return std::nullopt;

    offset_t tuple_offset = GetEndFreeSpace() - size;

    // Write header fields
    uint16_t old_slot_count = GetNumSlots();
    SetNumSlots(old_slot_count + 1);
    SetStartFreeSpace(GetStartFreeSpace() + SlotEntry::SIZE);
    SetEndFreeSpace(tuple_offset);

    // Write slot directory entry
    SetSlotOffset(old_slot_count, tuple_offset);
    SetSlotSize(old_slot_count, size);

    return old_slot_count;
}

void WritePage::WriteSlot(slot_id_t slot_id, std::span<const std::byte> data)
{
    assert(slot_id < GetNumSlots());
    assert(data.size_bytes() == GetSlotSize(slot_id));
    memcpy(page_data_m.data(), data.data(), sizeof(data));
}

void WritePage::DeleteSlot(slot_id_t slot_id)
{
    assert(slot_id < GetNumSlots());
    SetSlotDeleted(slot_id, true);
}

void WritePage::VacuumPage()
{
    struct SlotEntry {
        slot_id_t slot_id;
        offset_t offset;
        uint16_t size;
    };

    struct SlotEntryCompare {
        bool operator()(const SlotEntry& l, const SlotEntry& r)
        {
            return l.offset > r.offset;
        }
    };

    std::priority_queue<SlotEntry, std::vector<SlotEntry>, SlotEntryCompare> pq;

    for (slot_id_t id = 0; id < GetNumSlots(); id++) {
        SlotEntry slot_entry {
            .slot_id = id,
            .offset = GetOffset(id),
            .size = GetSlotSize(id),
        };

        if (IsSlotDeleted(id)) {
            SetSlotOffset(id, ::SlotEntry::RECLAIMED);
        } else {
            pq.push(slot_entry);
        }
    }

    offset_t freespace_end = PAGE_SIZE;
    while (!pq.empty()) {
        SlotEntry slot_entry = pq.top();
        pq.pop();

        freespace_end -= slot_entry.size;
        memmove(page_data_m.data() + freespace_end, page_data_m.data() + slot_entry.offset, slot_entry.size);
        SetSlotOffset(slot_entry.slot_id, freespace_end);
    }
    SetEndFreeSpace(freespace_end);
}

void WritePage::SetNumSlots(uint16_t num_slots)
{
    memcpy(page_data_m.data() + Header::Offsets::NUM_SLOTS, &num_slots, sizeof(num_slots));
}

void WritePage::SetStartFreeSpace(offset_t offset)
{
    memcpy(page_data_m.data() + Header::Offsets::FREE_START, &offset, sizeof(offset));
}

void WritePage::SetEndFreeSpace(offset_t offset)
{
    memcpy(page_data_m.data() + Header::Offsets::FREE_END, &offset, sizeof(offset));
}

void WritePage::SetSlotOffset(slot_id_t slot_id, offset_t offset)
{
    assert(slot_id < GetNumSlots());
    offset_t slot_offset_offset = Header::SIZE
        + slot_id * SlotEntry::SIZE
        + SlotEntry::Offsets::OFFSET;
    memcpy(page_data_m.data() + slot_offset_offset, &offset, sizeof(offset));
}

void WritePage::SetSlotSize(slot_id_t slot_id, uint16_t size)
{
    assert(slot_id < GetNumSlots());
    uint16_t slot_size_offset = Header::SIZE
        + slot_id * SlotEntry::SIZE
        + SlotEntry::Offsets::TUPLE_SIZE;
    memcpy(page_data_m.data() + slot_size_offset, &size, sizeof(size));
}

void WritePage::SetSlotDeleted(slot_id_t slot_id, bool deleted)
{
    uint8_t value = 1 ? deleted : 0;
    offset_t slot_deleted_offset = Header::SIZE
        + slot_id * SlotEntry::SIZE
        + SlotEntry::Offsets::DELETED;
    memcpy(page_data_m.data() + slot_deleted_offset, &value, sizeof(value));
}
