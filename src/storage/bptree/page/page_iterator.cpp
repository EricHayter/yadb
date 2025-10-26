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
    , is_end_m(slot_id >= page_m->GetSlotDirectoryCapacity())
    , cached_value_m()
{
    // If this is not an end iterator and we're starting at a deleted slot,
    // advance to the first valid slot
    if (!is_end_m && page_m != nullptr && slot_id_m < page_m->GetSlotDirectoryCapacity()) {
        if (page_m->IsSlotDeleted(slot_id_m)) {
            advance_to_next_valid();
        }
        // Update cached value if we're at a valid position
        if (!is_end_m) {
            update_value();
        }
    }
}

PageIterator::reference PageIterator::operator*() const
{
    return cached_value_m;
}

PageIterator::pointer PageIterator::operator->() const
{
    return &cached_value_m;
}

PageIterator& PageIterator::operator++()
{
    ++slot_id_m;
    advance_to_next_valid();
    if (!is_end_m) {
        update_value();
    }
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
    if (is_end_m) {
        is_end_m = false;
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
    if (!is_end_m) {
        update_value();
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
    // Both are end iterators
    if (is_end_m && other.is_end_m) {
        return page_m == other.page_m;
    }

    // One is end and the other isn't
    if (is_end_m != other.is_end_m) {
        return false;
    }

    // Both are regular iterators
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

    // If we've gone past the end, mark as end iterator
    if (slot_id_m >= capacity) {
        is_end_m = true;
    }
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
        is_end_m = true;
        slot_id_m = page_m->GetSlotDirectoryCapacity();
    }
}

void PageIterator::update_value() const
{
    // Note: const_cast is safe here because the caller is required to hold
    // the appropriate lock on the page before using the iterator
    auto data = const_cast<Page*>(page_m)->ReadSlot(slot_id_m);
    cached_value_m = std::make_pair(slot_id_m, data);
}
