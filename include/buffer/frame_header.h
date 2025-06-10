#pragma once
#include "common/type_definitions.h"
#include <shared_mutex>

struct FrameHeader
{
    FrameHeader(frame_id_t id, MutPageView data_view);

    FrameHeader(FrameHeader& other) = delete;
    FrameHeader& operator=(FrameHeader& other) = delete;
    FrameHeader(FrameHeader&& other) = delete;
    FrameHeader& operator=(FrameHeader&& other) = delete;

    MutPageView GetMutData() { return data; };
    PageView GetData() { return data; };

    frame_id_t id;
    MutPageView data;
    page_id_t page_id;

    // Has this page been altered?
    bool is_dirty{ false };

    // concurrent readers/writers
    int pin_count{ 0 };

    // mutex for safely sharing access to the buffer
    std::shared_mutex mut;
};

