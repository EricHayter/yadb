/*-----------------------------------------------------------------------------
 *
 * page.h
 *      Slotted page interface
 *
 * This header specifies the interface for pages which support
 * the basic operations of inserting and deleting into pages using a slotted
 * page format.
 *
 * As the pages themselves act as a layer of abstraction over page views,
 * the class supports thread-safe reads and writes through the buffer pool.
 *
 *-----------------------------------------------------------------------------
 */

#pragma once

#include <span>

#include <cstdint>

class PageBufferManager;
class Frame;

using page_id_t = uint32_t;

/* Size of ALL pages in the database. Maximum allowable value of 65536 due
 * to the constraints on definitions of offset and slot_id types */
constexpr std::size_t PAGE_SIZE = 4096;
using MutPageView = std::span<char, PAGE_SIZE>;
using PageView = std::span<const char, PAGE_SIZE>;

/* Page definition with all the basic read-only operations on pages. Such as
 * reading page header fields, reading the slot directory, and validating
 * checksum data. This class is intended to be built on top of for other types
 * of pages using the slotted page format e.g. B+ tree index nodes. */
class Page {
public:
    /* Constructors and Assignment */
    /* constructor for subclasses of base page */
    Page(PageBufferManager* page_buffer_manager, Frame* frame);
    /* Allow for transfer of ownership of pages */
    Page(Page&& other);
    Page& operator=(Page&& other);
    /* due to the calling of the AddAccessor and RemoveAccessor there must only
     * be a single owner to a given page i.e. no copies as this would result
     * in duplicate notifications to the page buffer manager. */
    Page(const Page& other) = delete;
    Page& operator=(const Page& other) = delete;
    ~Page();

    /* Locking and unlocking operations */
    void lock();
    bool try_lock();
    void unlock();
    void lock_shared();
    bool try_lock_shared();
    void unlock_shared();

    MutPageView GetMutView() const;
    PageView GetView() const;

    /* Page Metadata Operations */
    page_id_t GetPageId() const;

private:
    PageBufferManager* page_buffer_manager_m;
    Frame* frame_m;
};
