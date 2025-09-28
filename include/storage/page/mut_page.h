#pragma once

#include <mutex>
#include <optional>
#include <shared_mutex>

#include "storage/page/base_page.h"

/* Page handle for write access. Has all of the same functionality but also
 * support for updating page headers, slot directory entries, and writing data.
 */
class MutPage : public BasePage {
public:
    MutPage(PageBufferManager* buffer_manager, page_id_t page_id, MutPageView page_view, std::unique_lock<std::shared_mutex>&& lk);

    /* copy constructors will be disabled due to the data lock member */
    MutPage(const MutPage& other) = delete;
    MutPage& operator=(const MutPage& other) = delete;

    ~MutPage();

    /* initializes the fields inside of the page header. NOTE: This should only
     * be used when creating a page for the first time i.e. right after calling
     * NewPage with the page buffer manager. */
    void InitPage();

    /* Updates the checksum field in the page header. This MUST be called
     * before flushing pages as the checksum will be validated on page
     * load */
    void UpdateChecksum();

    /* returns the slot of a newly allocated tuple of size in the page. If no
     * space can be found for the tuple then returns std::nullopt. */
    std::optional<slot_id_t> AllocateSlot(uint16_t size);

    void WriteSlot(slot_id_t slot_id, std::span<const char> data);

    /* logically deleted tuple from page. Note this does not immediately free
     * up any space in general. To fully get back space from deletes the
     * page must be vacuumed. */
    void DeleteSlot(slot_id_t slot_id);

    /* Reacquires freed space from deleted tuples. This is a fairly expensive
     * operation use it sparingly. */
    void VacuumPage();

private:
    /* functions for updating page header fields */
    void SetNumSlots(uint16_t num_slots);
    void SetStartFreeSpace(offset_t offset);
    void SetEndFreeSpace(offset_t offset);
    void SetChecksum(uint64_t checksum);

    /* functions for updating slot directory entries */
    void SetSlotDeleted(slot_id_t slot_id, bool deleted);
    void SetSlotOffset(slot_id_t slot_id, offset_t offset);
    void SetSlotSize(slot_id_t slot_id, uint16_t size);

private:
    /* lock for ownership of page data in Page parent class */
    std::unique_lock<std::shared_mutex> data_lk_m;
};
