#include "buffer/frame_header.h"
#include "common/type_definitions.h"

FrameHeader::FrameHeader(frame_id_t id, MutPageView data_view)
    : id(id)
    , data(data_view)
{
}
