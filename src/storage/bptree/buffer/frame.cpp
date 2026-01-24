#include "buffer/frame.h"

Frame::Frame(frame_id_t id, MutFullPage data_view)
    : id(id)
    , data(data_view)
{
}
