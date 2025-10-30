/*-----------------------------------------------------------------------------
 *
 * page_iterator.cpp
 *      Implementation of PageIterator
 *
 *-----------------------------------------------------------------------------
 */

#include "page/page_iterator.h"
#include "page/page.h"

PageIterator::PageIterator(const Page* page, slot_id_t slot_id)
    : page_m(page)
    , slot_id_m(slot_id)
{
    // If this is not an end iterator and we're starting at a deleted slot,
    // advance to the first valid slot
    if (!is_end() && page_m != nullptr && page_m->IsSlotDeleted(slot_id_m)) {
        advance_to_next_valid();
    }
}

PageIterator::reference PageIterator::operator*() const
{
    return slot_id_m;
}

PageIterator& PageIterator::operator++()
{
    ++slot_id_m;
    advance_to_next_valid();
    return *this;
}

PageIterator PageIterator::operator++(int)
{
    PageIterator tmp = *this;
    ++(*this);
    return tmp;
}

PageIterator& PageIterator::operator--()
{
    // If we're at the end, move to the last valid slot
    if (is_end()) {
        if (page_m->GetSlotDirectoryCapacity() > 0) {
            slot_id_m = page_m->GetSlotDirectoryCapacity() - 1;
            retreat_to_previous_valid();
        }
    } else {
        if (slot_id_m > 0) {
            --slot_id_m;
            retreat_to_previous_valid();
        }
    }
    return *this;
}

PageIterator PageIterator::operator--(int)
{
    PageIterator tmp = *this;
    --(*this);
    return tmp;
}

bool PageIterator::operator==(const PageIterator& other) const
{
    // Two iterators are equal if they point to the same page and slot
    return page_m == other.page_m && slot_id_m == other.slot_id_m;
}

bool PageIterator::operator!=(const PageIterator& other) const
{
    return !(*this == other);
}

void PageIterator::advance_to_next_valid()
{
    const slot_id_t capacity = page_m->GetSlotDirectoryCapacity();

    // Advance past any deleted slots
    while (slot_id_m < capacity && page_m->IsSlotDeleted(slot_id_m)) {
        ++slot_id_m;
    }

    // If we've gone past the end, slot_id_m >= capacity naturally represents end
}

void PageIterator::retreat_to_previous_valid()
{
    // Move backwards past any deleted slots
    while (slot_id_m > 0 && page_m->IsSlotDeleted(slot_id_m)) {
        --slot_id_m;
    }

    // Check if the current slot is valid
    if (page_m->IsSlotDeleted(slot_id_m)) {
        // No valid slots found, this shouldn't normally happen
        // but handle it by becoming an end iterator
        slot_id_m = page_m->GetSlotDirectoryCapacity();
    }
}

bool PageIterator::is_end() const
{
    return page_m == nullptr || slot_id_m >= page_m->GetSlotDirectoryCapacity();
}
