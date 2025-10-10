#pragma once

#include "storage/bptree/page/base_page.h"

#include <shared_mutex>

using frame_id_t = uint32_t;

/**
 * A class to manage state of frames inside of the page buffer manager
 *
 * This class specifically tracks two important pieces of information for each
 * frame:
 * 1. page id: the page id of the page being contained in the frame
 * 2. "dirtiness": whether or not a frame has been written to (if that's the
 *    case then the frame data must be written to disk when the page is
 *    released from the frame)
 * 3. pin count: tracking how many accessors there are for this particular
 *    frame. Not only indicates when a frame is in use but also if a writable
 *    view can be created for the frame.
 */
struct Frame {
    Frame(frame_id_t id, MutPageView data_view);

    Frame(Frame& other) = delete;
    Frame& operator=(Frame& other) = delete;
    Frame(Frame&& other) = delete;
    Frame& operator=(Frame&& other) = delete;

    MutPageView GetMutData() { return data; };
    PageView GetData() { return data; };

    frame_id_t id;

    /* The associated page that the frame is storing. */
    page_id_t page_id;

    /* Has this page been written to? */
    bool is_dirty { false };

    /* concurrent readers/writers */
    int pin_count { 0 };

    /* Mutex on the underlying data for the frame */
    std::shared_mutex mut;

    /* A mutable view into the buffer provided by the page buffer manager. */
    MutPageView data;
};
