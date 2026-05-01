#pragma once

#include "storage/on_disk/types.h"

/* Heap Page
 * Pages in the heap file will have a slightly tweaked version of the slotted
 * page format with additional information that is not useful for the slotted
 * pages but neccessary for heap file traversal. In particular the heap file
 * will be organized as two doubly linked lists. One for "full pages"  and
 * another for pages with freespace. To acommodate this, two page pointers
 * will be added to the end of heap file pages as next and previous page
 * pointers, using NULL_PAGE_ID.
 *
 * There will be one exception to this format being the "header" page. This
 * will be a full page but contain a pair of pointers one pointing to the
 * doubly linked list of full pages, and the other pointing to the doubly
 * linked list of page will freespace.
 *
 * Heap File Organization:
 *
 *     Header Page (page 0)
 *     ┌────────────────┐
 *     │ next ───────┐  │  Points to head of full pages list
 *     │ prev ─────┐ │  │  Points to head of partial pages list
 *     └───────────┼─┼──┘
 *                 │ │
 *        ┌────────┘ └────────┐
 *        │                   │
 *        ▼                   ▼
 *   Partial Pages       Full Pages
 *   (with freespace)    (no freespace)
 *
 *   ┌──────────┐       ┌──────────┐
 *   │ Page A   │◄─────►│ Page X   │
 *   │ next/prev│       │ next/prev│
 *   └──────────┘       └──────────┘
 *        ▲▼                 ▲▼
 *   ┌──────────┐       ┌──────────┐
 *   │ Page B   │◄─────►│ Page Y   │
 *   │ next/prev│       │ next/prev│
 *   └──────────┘       └──────────┘
 *        ▲▼                 ▲▼
 *       ...                ...
 *
 * Each list is doubly-linked with NULL_PAGE_ID (-1) marking the end.
 * When a page becomes full, it moves from partial → full list.
 * When a page with deletions occurs, it moves from full → partial list.
 */

namespace heap_page {

// Allocates space for the pair of navigation pointers in each slotted page
// all calls the required slotted page initalize function.
void InitPage(MutFullPage page);
void SetNextPage(MutFullPage page, page_id_t page_id);
void SetPrevPage(MutFullPage page, page_id_t page_id);
page_id_t GetNextPage(FullPage page);
page_id_t GetPrevPage(FullPage page);

// Gets the head of the doubly linked list of full pages
inline page_id_t GetFullPagesHead(FullPage page) { return GetNextPage(page); }

// Gets the head of the doubly linked list of partially full pages
inline page_id_t GetPartialPagesHead(FullPage page) { return GetPrevPage(page); }

};
