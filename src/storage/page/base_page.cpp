#include "storage/page/base_page.h"

#include <iostream>
#include <stdexcept>

#include "buffer/page_buffer_manager.h"
#include "storage/page/checksum.h"

BasePage::~BasePage()
{
    if (buffer_manager_m)
        buffer_manager_m->RemoveAccessor(page_id_m);
}

BasePage::BasePage(PageBufferManager* buffer_manager, page_id_t page_id, MutPageView page_view, bool is_writer, bool fresh_page)
    : buffer_manager_m { buffer_manager }
    , page_id_m { page_id }
    , page_data_m { page_view }
{
    buffer_manager_m->AddAccessor(page_id, is_writer);
}

bool BasePage::ValidChecksum() const
{
    return checksum64(page_data_m) == 0x00;
}

PageType BasePage::GetPageType() const
{
    PageType page_type;
    memcpy(&page_type, page_data_m.data() + Header::Offsets::PAGE_TYPE, sizeof(PageType));
    return page_type;
}

uint16_t BasePage::GetNumSlots() const
{
    uint16_t num_slots;
    memcpy(&num_slots, page_data_m.data() + Header::Offsets::NUM_SLOTS, sizeof(uint16_t));
    return num_slots;
}

offset_t BasePage::GetFreeSpaceSize() const
{
    return GetEndFreeSpace() - GetStartFreeSpace();
}

offset_t BasePage::GetStartFreeSpace() const
{
    offset_t start_free_space;
    memcpy(&start_free_space, page_data_m.data() + Header::Offsets::FREE_START, sizeof(offset_t));
    return start_free_space;
}

offset_t BasePage::GetEndFreeSpace() const
{
    offset_t end_free_space;
    memcpy(&end_free_space, page_data_m.data() + Header::Offsets::FREE_END, sizeof(offset_t));
    return end_free_space;
}

uint64_t BasePage::GetChecksum() const
{
    uint64_t checksum;
    memcpy(&checksum, page_data_m.data() + Header::Offsets::CHECKSUM, sizeof(uint64_t));
    return checksum;
}

std::span<const char> BasePage::ReadSlot(slot_id_t slot_id)
{
    if (IsSlotDeleted(slot_id))
        throw std::runtime_error(std::format("Slot {} is deleted", slot_id));
    return page_data_m.subspan(GetOffset(slot_id), GetSlotSize(slot_id));
}

void BasePage::PrintPage() const
{
    std::cout << "Page type: " << static_cast<uint8_t>(GetPageType()) << '\n';
    std::cout << "Number of slots: " << GetNumSlots() << '\n';
    std::cout << "Free space start: " << GetStartFreeSpace() << '\n';
    std::cout << "Free space end: " << GetEndFreeSpace() << '\n';
    std::cout << "Checksum: " << GetChecksum() << "\n\n";

    constexpr std::size_t bytes_per_line = 16;
    for (std::size_t i = 0; i < page_data_m.size(); i += bytes_per_line) {
        // Print offset
        std::cout << std::setw(6) << std::setfill('0') << std::hex << i << "  ";

        // Hex bytes
        for (std::size_t j = 0; j < bytes_per_line; ++j) {
            if (i + j < page_data_m.size()) {
                unsigned byte = static_cast<unsigned char>(page_data_m[i + j]);
                std::cout << std::setw(2) << byte << ' ';
            } else {
                std::cout << "   ";  // padding
            }
        }

        // ASCII view
        std::cout << " |";
        for (std::size_t j = 0; j < bytes_per_line && i + j < page_data_m.size(); ++j) {
            unsigned char c = static_cast<unsigned char>(page_data_m[i + j]);
            std::cout << (std::isprint(c) ? static_cast<char>(c) : '.');
        }
        std::cout << "|\n";
    }

    // Reset stream flags
    std::cout << std::dec << std::setfill(' ');
}

bool BasePage::IsSlotDeleted(slot_id_t slot_id) const
{
    uint8_t deleted;
    offset_t deleted_offset = Header::SIZE + slot_id * SlotEntry::SIZE + SlotEntry::Offsets::DELETED;
    memcpy(&deleted, page_data_m.data() + deleted_offset, sizeof(deleted));
    return deleted > 0;
}

offset_t BasePage::GetOffset(slot_id_t id) const
{
    assert(id >= 0 && id < GetNumSlots());
    offset_t offset;
    offset_t slot_entry_offset = Header::SIZE + id * SlotEntry::SIZE + SlotEntry::Offsets::OFFSET;
    memcpy(&offset, page_data_m.data() + slot_entry_offset, sizeof(offset));
    return offset;
}

uint16_t BasePage::GetSlotSize(slot_id_t slot_id) const
{
    assert(slot_id >= 0 && slot_id < GetNumSlots());
    uint16_t slot_size;
    offset_t slot_entry_offset = Header::SIZE + slot_id * SlotEntry::SIZE + SlotEntry::Offsets::TUPLE_SIZE;
    memcpy(&slot_size, page_data_m.data() + slot_entry_offset, sizeof(slot_size));
    return slot_size;
}
