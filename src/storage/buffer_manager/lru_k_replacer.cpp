#include "storage/buffer_manager/lru_k_replacer.h"
#include "storage/buffer_manager/frame.h"

LRUKReplacer::LRUFrameHistory::LRUFrameHistory(frame_id_t frame_id)
    : frame_id(frame_id)
{
}

void LRUKReplacer::RegisterFrame(frame_id_t frame_id)
{
    frames_m.emplace(frame_id, LRUFrameHistory(frame_id));
    evictable_count_m++;
}

std::optional<frame_id_t> LRUKReplacer::EvictFrame()
{
    if (frames_m.empty()) {
        return std::nullopt;
    }

    bool found_short_history { false };
    std::optional<frame_id_t> to_remove {};

    for (const auto& [frame_id, frame] : frames_m) {
        if (!frame.is_evictable)
            continue;

        // TODO can integrate this into the logic on line 46 below
        if (frame.history.empty()) {
            // Reset history before returning
            frames_m[frame_id].history.clear();
            return frame_id;
        }

        if (!to_remove.has_value()) {
            to_remove = frame_id;
            found_short_history = frame.history.size() < k_m;
            continue;
        }

        bool current_is_short = frame.history.size() < k_m;
        const auto& candidate = frames_m[*to_remove];

        if (current_is_short && !found_short_history) {
            to_remove = frame_id;
            found_short_history = true;
        } else if (current_is_short == found_short_history) {
            // Prefer the older one
            if ((current_is_short && frame.history.front() < candidate.history.front()) || (!current_is_short && frame.history.back() < candidate.history.back())) {
                to_remove = frame_id;
            }
        }
    }

    if (to_remove.has_value()) {
        // Clear the history before eviction
        frames_m[*to_remove].history.clear();
    }

    return to_remove;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id)
{
    // TODO make this an asssert or spdlog it or something??
    if (not frames_m.contains(frame_id))
        return;
    LRUFrameHistory& frame = frames_m[frame_id];

    frame.history.push_back(current_timestamp_m);
    if (frame.history.size() > k_m)
        frame.history.pop_front();

    current_timestamp_m++;
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
