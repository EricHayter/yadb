#include "storage/on_disk/buffer_manager/frame.h"

Frame::Frame(frame_id_t frame_id, MutFullPage data_view)
    : id(frame_id)
    , data(data_view)
{
}
