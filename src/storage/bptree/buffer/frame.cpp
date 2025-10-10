#include "buffer/frame.h"

Frame::Frame(frame_id_t id, MutPageView data_view)
    : id(id)
    , data(data_view)
{
}
