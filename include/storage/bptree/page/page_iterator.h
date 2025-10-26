/*-----------------------------------------------------------------------------
 *
 * page_iterator.h
 *      Bidirectional iterator for slotted pages
 *
 * This header provides a bidirectional iterator that allows iteration over
 * non-deleted slots in a slotted page. The iterator conforms to C++20
 * bidirectional_iterator requirements and can be used with standard algorithms.
 *
 * The iterator yields std::pair<slot_id_t, std::span<const char>> where:
 *  - first: the slot ID
 *  - second: a span to the slot's data
 *
 *-----------------------------------------------------------------------------
 */

#pragma once

#include "page/page_layout.h"
#include <iterator>
#include <span>

class Page;

/**
 * \brief Bidirectional iterator for iterating over non-deleted slots in a Page
 *
 * This iterator allows forward and backward traversal of slots in a slotted page,
 * automatically skipping deleted slots. It conforms to the C++20 bidirectional_iterator
 * concept and can be used with standard algorithms like std::find_if, std::copy, etc.
 *
 * Usage example:
 * \code
 *   Page page = ...;
 *   for (auto [slot_id, data] : page) {
 *       // Process slot_id and data span
 *   }
 * \endcode
 */
class PageIterator {
public:
    // Iterator traits (C++20 style)
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::pair<slot_id_t, std::span<const char>>;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    /**
     * \brief Default constructor (creates an end iterator)
     */
    PageIterator() = default;

    /**
     * \brief Construct a PageIterator
     * \param page Pointer to the page to iterate over
     * \param slot_id Starting slot ID
     * \param is_end Whether this is an end iterator
     */
    PageIterator(const Page* page, slot_id_t slot_id);

    /**
     * \brief Dereference operator
     * \return A reference to the pair of (slot_id, data span)
     */
    reference operator*() const;

    /**
     * \brief Arrow operator
     * \return A pointer to the current value
     */
    pointer operator->() const;

    /**
     * \brief Pre-increment: move to next non-deleted slot
     * \return Reference to this iterator
     */
    PageIterator& operator++();

    /**
     * \brief Post-increment: move to next non-deleted slot
     * \return Copy of iterator before increment
     */
    PageIterator operator++(int);

    /**
     * \brief Pre-decrement: move to previous non-deleted slot
     * \return Reference to this iterator
     */
    PageIterator& operator--();

    /**
     * \brief Post-decrement: move to previous non-deleted slot
     * \return Copy of iterator before decrement
     */
    PageIterator operator--(int);

    /**
     * \brief Equality comparison
     */
    bool operator==(const PageIterator& other) const;

    /**
     * \brief Inequality comparison
     */
    bool operator!=(const PageIterator& other) const;

private:
    /**
     * \brief Advance to the next non-deleted slot
     */
    void advance_to_next_valid();

    /**
     * \brief Move back to the previous non-deleted slot
     */
    void retreat_to_previous_valid();

    /**
     * \brief Update the cached value to reflect current slot
     */
    void update_value() const;

    const Page* page_m = nullptr;
    slot_id_t slot_id_m = 0;
    bool is_end_m = true;
    mutable value_type cached_value_m {}; // Cached value for reference stability
};

// C++20 iterator concept verification (will be checked at compile time)
static_assert(std::bidirectional_iterator<PageIterator>,
    "PageIterator must satisfy bidirectional_iterator concept");
