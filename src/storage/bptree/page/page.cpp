#include "page/page.h"

#include <cassert>

#include <cstring>
#include <format>
#include <iomanip>
#include <iostream>
#include <queue>

#include "buffer/page_buffer_manager.h"
#include "core/shared_spinlock.h"
#include "page/checksum.h"
#include "page/page_layout.h"

Page::~Page()
{
    if (page_buffer_manager_m)
        page_buffer_manager_m->RemoveAccessor(frame_m->page_id);
}

Page::Page(PageBufferManager* page_buffer_manager, Frame* frame)
    : frame_m { frame }
    , page_buffer_manager_m { page_buffer_manager }

{   if (page_buffer_manager)
        page_buffer_manager_m->AddAccessor(frame->page_id);
}

Page::Page(Page&& other)
    : frame_m { std::move(other.frame_m) }
    , page_buffer_manager_m { std::move(other.page_buffer_manager_m) }
{
    // Null out the moved-from object to prevent double RemoveAccessor call
    other.frame_m = nullptr;
    other.page_buffer_manager_m = nullptr;
}

Page& Page::operator=(Page&& other)
{
    if (&other != this) {
        // Call RemoveAccessor for the current object before replacing
        if (page_buffer_manager_m) {
            page_buffer_manager_m->RemoveAccessor(frame_m->page_id);
        }

        frame_m = std::move(other.frame_m);
        page_buffer_manager_m = std::move(other.page_buffer_manager_m);

        // Null out the moved-from object to prevent double RemoveAccessor call
        other.frame_m = nullptr;
        other.page_buffer_manager_m = nullptr;
    }
    return *this;
}

bool Page::ValidChecksum() const
{
    return checksum64(frame_m->data) == 0x00;
}

PageType Page::GetPageType() const
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);
    PageType page_type;
    memcpy(&page_type, frame_m->data.data() + Header::Offsets::PAGE_TYPE, sizeof(page_type));
    return page_type;
}

uint16_t Page::GetNumSlots() const
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);
    uint16_t num_slots;
    memcpy(&num_slots, frame_m->data.data() + Header::Offsets::NUM_SLOTS, sizeof(num_slots));
    return num_slots;
}

offset_t Page::GetFreeSpaceSize() const
{
    return GetEndFreeSpace() - GetStartFreeSpace();
}

bool Page::CanInsert(uint16_t size, bool reuse_deleted) const
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);

    /* Check if we can reuse a deleted slot */
    if (reuse_deleted) {
        std::optional<slot_id_t> slot_id;
        for (slot_id_t i = 0; i < GetSlotDirectoryCapacity(); i++) {
            if (IsSlotDeleted(i)) {
                slot_id = i;
                break;
            }
        }

        /* If we found a deleted slot to reuse */
        if (slot_id) {
            /* If new size fits in the old slot's space, we can reuse it */
            if (size < GetSlotSize(*slot_id)) {
                return true;
            }
            /* Otherwise, check if we have enough free space for the new data */
            return size <= GetFreeSpaceSize();
        }
    }

    /* No deleted slots to reuse (or not allowed), need space for both slot entry and data */
    return GetFreeSpaceSize() >= SlotEntry::SIZE + size;
}

offset_t Page::GetStartFreeSpace() const
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);
    offset_t start_free_space;
    memcpy(&start_free_space, frame_m->data.data() + Header::Offsets::FREE_START, sizeof(start_free_space));
    return start_free_space;
}

offset_t Page::GetEndFreeSpace() const
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);
    offset_t end_free_space;
    memcpy(&end_free_space, frame_m->data.data() + Header::Offsets::FREE_END, sizeof(end_free_space));
    return end_free_space;
}

uint64_t Page::GetChecksum() const
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);
    uint64_t checksum;
    memcpy(&checksum, frame_m->data.data() + Header::Offsets::CHECKSUM, sizeof(checksum));
    return checksum;
}

std::span<const char> Page::ReadSlot(slot_id_t slot_id)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);
    assert(!IsSlotDeleted(slot_id) && std::format("Slot {} is deleted", slot_id).data());
    return frame_m->data.subspan(GetOffset(slot_id), GetSlotSize(slot_id));
}

void Page::PrintPage() const
{
    std::cout << "Page type: " << static_cast<uint8_t>(GetPageType()) << '\n';
    std::cout << "Number of slots: " << GetNumSlots() << '\n';
    std::cout << "Free space start: " << GetStartFreeSpace() << '\n';
    std::cout << "Free space end: " << GetEndFreeSpace() << '\n';
    std::cout << "Checksum: " << GetChecksum() << "\n\n";

    constexpr std::size_t bytes_per_line = 16;
    for (std::size_t i = 0; i < frame_m->data.size(); i += bytes_per_line) {
        // Print offset
        std::cout << std::setw(6) << std::setfill('0') << std::hex << i << "  ";

        // Hex bytes
        for (std::size_t j = 0; j < bytes_per_line; ++j) {
            if (i + j < frame_m->data.size()) {
                unsigned byte = static_cast<unsigned char>(frame_m->data[i + j]);
                std::cout << std::setw(2) << byte << ' ';
            } else {
                std::cout << "   "; // padding
            }
        }

        // ASCII view
        std::cout << " |";
        for (std::size_t j = 0; j < bytes_per_line && i + j < frame_m->data.size(); ++j) {
            unsigned char c = static_cast<unsigned char>(frame_m->data[i + j]);
            std::cout << (std::isprint(c) ? static_cast<char>(c) : '.');
        }
        std::cout << "|\n";
    }

    // Reset stream flags
    std::cout << std::dec << std::setfill(' ');
}

uint16_t Page::GetSlotDirectoryCapacity() const
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);
    return (GetStartFreeSpace() - Header::SIZE) / SlotEntry::SIZE;
}

bool Page::IsSlotDeleted(slot_id_t slot_id) const
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);
    assert(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity());
    uint8_t deleted;
    offset_t deleted_offset = Header::SIZE + slot_id * SlotEntry::SIZE + SlotEntry::Offsets::DELETED;
    memcpy(&deleted, frame_m->data.data() + deleted_offset, sizeof(deleted));
    return deleted > 0;
}

offset_t Page::GetOffset(slot_id_t slot_id) const
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);
    assert(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity());
    offset_t offset;
    offset_t slot_entry_offset = Header::SIZE + slot_id * SlotEntry::SIZE + SlotEntry::Offsets::OFFSET;
    memcpy(&offset, frame_m->data.data() + slot_entry_offset, sizeof(offset));
    return offset;
}

uint16_t Page::GetSlotSize(slot_id_t slot_id) const
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE || frame_m->mut.State() == SharedSpinlock::LockState::SHARED);
    assert(slot_id >= 0 && slot_id < GetSlotDirectoryCapacity());
    uint16_t slot_size;
    offset_t slot_entry_offset = Header::SIZE + slot_id * SlotEntry::SIZE + SlotEntry::Offsets::TUPLE_SIZE;
    memcpy(&slot_size, frame_m->data.data() + slot_entry_offset, sizeof(slot_size));
    return slot_size;
}

void Page::InitPage(PageType page_type)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    SetNumSlots(0);
    SetStartFreeSpace(Header::SIZE);
    SetEndFreeSpace(PAGE_SIZE);
    SetPageType(page_type);
}

void Page::UpdateChecksum()
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    SetChecksum(0x00);
    uint64_t new_checksum = checksum64(frame_m->data);
    SetChecksum(new_checksum);
}

std::optional<slot_id_t> Page::AllocateSlot(uint16_t size)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    std::optional<slot_id_t> slot_id;
    /* try and reuse a deleted entry first */
    for (slot_id_t i = 0; i < GetSlotDirectoryCapacity(); i++) {
        if (IsSlotDeleted(i)) {
            slot_id = i;
        }
    }

    /* can't reuse anything just create a new entry */
    if (!slot_id) {
        /* not enough space for the entry */
        if (GetFreeSpaceSize() < SlotEntry::SIZE + size) {
            return std::nullopt;
        }

        uint16_t old_slot_count = GetNumSlots();
        SetNumSlots(old_slot_count + 1);
        SetStartFreeSpace(GetStartFreeSpace() + SlotEntry::SIZE);

        offset_t tuple_offset = GetEndFreeSpace() - size;
        SetEndFreeSpace(tuple_offset);

        /* Write slot directory entry */
        SetSlotOffset(old_slot_count, tuple_offset);
        SetSlotSize(old_slot_count, size);

        return GetSlotDirectoryCapacity() - 1;
    }

    /* we reuse the slot */
    if (size < GetSlotSize(*slot_id)) {
        SetSlotSize(*slot_id, size);
        SetSlotDeleted(*slot_id, false);
    } else {
        if (size > GetFreeSpaceSize()) {
            return std::nullopt;
        }

        offset_t tuple_offset = GetEndFreeSpace() - size;
        SetEndFreeSpace(tuple_offset);

        /* Write slot directory entry */
        SetSlotOffset(*slot_id, tuple_offset);
        SetSlotSize(*slot_id, size);
    }

    return *slot_id;
}

std::optional<slot_id_t> Page::AppendSlot(uint16_t size)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);

    /* Check if there's enough space for both slot directory entry and tuple data */
    if (GetFreeSpaceSize() < SlotEntry::SIZE + size) {
        return std::nullopt;
    }

    /* Get the current slot count - this will be the new slot's ID */
    slot_id_t new_slot_id = GetSlotDirectoryCapacity();

    /* Update slot count */
    SetNumSlots(GetNumSlots() + 1);

    /* Grow slot directory downward */
    SetStartFreeSpace(GetStartFreeSpace() + SlotEntry::SIZE);

    /* Allocate tuple space from the end, growing upward */
    offset_t tuple_offset = GetEndFreeSpace() - size;
    SetEndFreeSpace(tuple_offset);

    /* Write slot directory entry */
    SetSlotOffset(new_slot_id, tuple_offset);
    SetSlotSize(new_slot_id, size);
    SetSlotDeleted(new_slot_id, false);

    return new_slot_id;
}

std::optional<slot_id_t> Page::ShiftSlotsRight(PageIterator position, uint16_t count)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);

    /* Check if there's enough free space for additional slot directory entries */
    size_t required_space = count * SlotEntry::SIZE;
    if (GetFreeSpaceSize() < required_space) {
        return std::nullopt;
    }

    /* Handle the end() case - just append new slots at the end */
    if (position == end()) {
        slot_id_t first_new_slot = GetSlotDirectoryCapacity();

        /* Grow the slot directory downward by count slots */
        SetStartFreeSpace(GetStartFreeSpace() + count * SlotEntry::SIZE);

        /* Initialize the new slots as empty (size 0, deleted) */
        for (uint16_t i = 0; i < count; ++i) {
            slot_id_t new_slot = first_new_slot + i;
            SetSlotOffset(new_slot, 0);
            SetSlotSize(new_slot, 0);
            SetSlotDeleted(new_slot, true);
        }

        return first_new_slot;
    }

    /* Handle the normal case - shift existing slots right */
    slot_id_t start_index = *position;
    assert(start_index <= GetSlotDirectoryCapacity());

    /* Get the current capacity before the shift */
    slot_id_t old_capacity = GetSlotDirectoryCapacity();

    /* Grow the slot directory downward */
    SetStartFreeSpace(GetStartFreeSpace() + count * SlotEntry::SIZE);

    /* Copy slot entries from right to left to avoid overwriting
     * Move entries at [start_index, old_capacity) to [start_index + count, old_capacity + count) */
    for (slot_id_t i = old_capacity; i > start_index; --i) {
        slot_id_t src_slot = i - 1;
        slot_id_t dst_slot = src_slot + count;

        /* Copy slot entry fields */
        offset_t offset = GetOffset(src_slot);
        uint16_t size = GetSlotSize(src_slot);
        bool deleted = IsSlotDeleted(src_slot);

        SetSlotOffset(dst_slot, offset);
        SetSlotSize(dst_slot, size);
        SetSlotDeleted(dst_slot, deleted);

        /* update the size of the src slots to 0 so that there aren't two
         * pointers to the same chunk in memory */
        SetSlotSize(src_slot, 0x00);
    }

    /* Initialize the newly opened slots as empty */
    for (uint16_t i = 0; i < count; ++i) {
        slot_id_t new_slot = start_index + i;
        SetSlotOffset(new_slot, 0);
        SetSlotSize(new_slot, 0);
        SetSlotDeleted(new_slot, true);
    }

    /* Return the first newly created slot */
    return start_index;
}

void Page::WriteSlot(slot_id_t slot_id, std::span<const char> data)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    assert(!IsSlotDeleted(slot_id));
    assert(data.size_bytes() == GetSlotSize(slot_id));
    memcpy(frame_m->data.data() + GetOffset(slot_id), data.data(), data.size_bytes());
}

bool Page::ResizeSlot(slot_id_t slot_id, uint16_t size)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);

    bool was_deleted = IsSlotDeleted(slot_id);
    auto old_slot_size = GetSlotSize(slot_id);

    /* If slot is deleted, treat it as having no old data */
    if (was_deleted) {
        old_slot_size = 0;
        SetSlotDeleted(slot_id, false);  // Mark as not deleted
        SetNumSlots(GetNumSlots() + 1);  // Increment slot count
    }

    /* can reuse old buffer just update offset */
    if (old_slot_size >= size) {
        SetSlotSize(slot_id, size);
        SetSlotOffset(slot_id, GetOffset(slot_id) + (old_slot_size - size));
        return true;
    }

    /* allocate new buffer (if possible) */
    if (GetFreeSpaceSize() < size)
        return false;

    SetSlotSize(slot_id, size);
    offset_t new_freespace_end = GetEndFreeSpace() - size;
    SetEndFreeSpace(new_freespace_end);
    SetSlotOffset(slot_id, new_freespace_end);
    return true;
}

void Page::DeleteSlot(slot_id_t slot_id)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    assert(slot_id < GetSlotDirectoryCapacity());
    SetSlotDeleted(slot_id, true);
    SetNumSlots(GetNumSlots() - 1);
}

void Page::VacuumPage()
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
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

    for (slot_id_t id = 0; id < GetSlotDirectoryCapacity(); id++) {
        SlotEntry slot_entry {
            .slot_id = id,
            .offset = GetOffset(id),
            .size = GetSlotSize(id),
        };

        if (IsSlotDeleted(id)) {
            SetSlotSize(id, 0);
        } else {
            pq.push(slot_entry);
        }
    }

    offset_t freespace_end = PAGE_SIZE;
    while (!pq.empty()) {
        SlotEntry slot_entry = pq.top();
        pq.pop();

        freespace_end -= slot_entry.size;
        memmove(frame_m->data.data() + freespace_end, frame_m->data.data() + slot_entry.offset, slot_entry.size);
        SetSlotOffset(slot_entry.slot_id, freespace_end);
    }
    SetEndFreeSpace(freespace_end);
}

void Page::SetNumSlots(uint16_t num_slots)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    memcpy(frame_m->data.data() + Header::Offsets::NUM_SLOTS, &num_slots, sizeof(num_slots));
}

void Page::SetStartFreeSpace(offset_t offset)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    memcpy(frame_m->data.data() + Header::Offsets::FREE_START, &offset, sizeof(offset));
}

void Page::SetEndFreeSpace(offset_t offset)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    memcpy(frame_m->data.data() + Header::Offsets::FREE_END, &offset, sizeof(offset));
}

void Page::SetChecksum(uint64_t checksum)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    memcpy(frame_m->data.data() + Header::Offsets::CHECKSUM, &checksum, sizeof(checksum));
}

void Page::SetPageType(PageType page_type)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    memcpy(frame_m->data.data() + Header::Offsets::PAGE_TYPE, &page_type, sizeof(page_type));
}

void Page::SetSlotOffset(slot_id_t slot_id, offset_t offset)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    assert(slot_id < GetSlotDirectoryCapacity());
    offset_t slot_offset_offset = Header::SIZE
        + slot_id * SlotEntry::SIZE
        + SlotEntry::Offsets::OFFSET;
    memcpy(frame_m->data.data() + slot_offset_offset, &offset, sizeof(offset));
}

void Page::SetSlotSize(slot_id_t slot_id, uint16_t size)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    assert(slot_id < GetSlotDirectoryCapacity());
    uint16_t slot_size_offset = Header::SIZE
        + slot_id * SlotEntry::SIZE
        + SlotEntry::Offsets::TUPLE_SIZE;
    memcpy(frame_m->data.data() + slot_size_offset, &size, sizeof(size));
}

void Page::SetSlotDeleted(slot_id_t slot_id, bool deleted)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    uint8_t value = 1 ? deleted : 0;
    offset_t slot_deleted_offset = Header::SIZE
        + slot_id * SlotEntry::SIZE
        + SlotEntry::Offsets::DELETED;
    memcpy(frame_m->data.data() + slot_deleted_offset, &value, sizeof(value));
}

PageIterator Page::begin() const
{
    return PageIterator(this, 0);
}

PageIterator Page::end() const
{
    return PageIterator(this, GetSlotDirectoryCapacity());
}
