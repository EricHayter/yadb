#include "storage/bptree/buffer/frame_header.h"

FrameHeader::FrameHeader(frame_id_t id, MutPageView data_view)
    : id(id)
    , data(data_view)
{
}
