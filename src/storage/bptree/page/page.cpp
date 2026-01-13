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

Page::~Page()
{
    if (page_buffer_manager_m)
        page_buffer_manager_m->RemoveAccessor(frame_m->page_id);
}

Page::Page(PageBufferManager* page_buffer_manager, Frame* frame)
    : frame_m { frame }
    , page_buffer_manager_m { page_buffer_manager }
{
}

Page::Page(Page&& other)
    : frame_m { other.frame_m }
    , page_buffer_manager_m { other.page_buffer_manager_m }
{
    other.frame_m = nullptr;
    other.page_buffer_manager_m = nullptr;
}

Page& Page::operator=(Page&& other)
{
    if (&other != this) {
        frame_m = other.frame_m;
        other.frame_m = nullptr;
        page_buffer_manager_m = other.page_buffer_manager_m;
        other.page_buffer_manager_m = nullptr;
    }
    return *this;
}

void Page::lock()
{
    frame_m->mut.lock();
}

bool Page::try_lock()
{
    return frame_m->mut.try_lock();
}

void Page::unlock()
{
    frame_m->mut.unlock();
}

void Page::lock_shared()
{
    frame_m->mut.lock_shared();
}

bool Page::try_lock_shared()
{
    return frame_m->mut.try_lock_shared();
}

void Page::unlock_shared()
{
    frame_m->mut.unlock_shared();
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

void Page::InitPage()
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    SetNumSlots(0);
    SetStartFreeSpace(Header::SIZE);
    SetEndFreeSpace(PAGE_SIZE);
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

void Page::WriteSlot(slot_id_t slot_id, std::span<const char> data)
{
    assert(frame_m->mut.State() == SharedSpinlock::LockState::EXCLUSIVE);
    assert(!IsSlotDeleted(slot_id));
    assert(data.size_bytes() == GetSlotSize(slot_id));
    memcpy(frame_m->data.data() + GetOffset(slot_id), data.data(), data.size_bytes());
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
