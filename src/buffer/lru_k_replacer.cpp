#include "buffer/lru_k_replacer.h"
#include "common/type_definitions.h"

std::optional<frame_id_t> LRUKReplacer::EvictFrame()
{
    if (frames_m.empty())
        return std::nullopt;

    // Frames with history of less than size k have been found
    bool found_short_history { false };
    std::optional<frame_id_t> to_remove {};

    // keep track of which has the smallest value of k
    for (const auto& [frame_id, frame] : frames_m) {
        // we cannot evict frames that are not evictable
        if (not frame.is_evictable)
            continue;

        if (not to_remove.has_value()) {
            to_remove = frame_id;
            found_short_history = frame.history.size() < k_m;
            continue;
        }

        bool current_is_short = frame.history.size() < k_m;

        // found first short history
        if (current_is_short && not found_short_history) {
            // Prefer frames with short history
            to_remove = frame_id;
            found_short_history = true;
        } else if (current_is_short == found_short_history) {
            // Both candidates are in the same category; pick the oldest
            const auto& candidate = frames_m[*to_remove];
            if ((current_is_short && frame.history.front() < candidate.history.front()) || (!current_is_short && frame.history.back() < candidate.history.back())) {
                to_remove = frame_id;
            }
        }
    }

    return to_remove;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id)
{
    if (not frames_m.contains(frame_id))
        return;
    LRUFrame& frame = frames_m[frame_id];

    frame.history.push_back(current_timestamp_m);
    if (frame.history.size() > k_m)
        frame.history.pop_front();

    current_timestamp_m++;
}

void LRUKReplacer::Remove(frame_id_t frame_id)
{
    if (not frames_m.contains(frame_id))
        return;
    frames_m.erase(frame_id);
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool evictable)
{
    if (not frames_m.contains(frame_id))
        return;

    // update our count of evictable frames
    if (evictable)
        evictable_count_m++;
    else
        evictable_count_m--;

    frames_m[frame_id].is_evictable = evictable;
}

std::size_t LRUKReplacer::GetEvictableCount()
{
    return evictable_count_m;
}
