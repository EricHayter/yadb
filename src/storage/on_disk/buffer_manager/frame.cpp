#include "storage/on_disk/buffer_manager/frame.h"
#include "storage/on_disk/constants.h"

Frame::Frame(frame_id_t frame_id, MutFullPage data_view)
    : id(frame_id)
    // Start with an invalid page id so a never-loaded frame doesn't alias a
    // real page: LoadPage erases frame->fp_id from the page map on reuse, and a
    // zero-initialized fp_id would spuriously evict page {0, 0}.
    , fp_id { INVALID_FILE_ID, NULL_PAGE_ID }
    , data(data_view)
{
}
