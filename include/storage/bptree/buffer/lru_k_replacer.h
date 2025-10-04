#pragma once

#include <deque>
#include <optional>
#include <unordered_map>

#include <ctime>

#include "buffer/frame_header.h"

/**
 * LRU-K Replacement Policy
 *
 * To implement the page buffer/cache a policy must be created to determine
 * which pages should be "evicted" or removed from the buffer in place of new
 * pages that need to be added.
 *
 * LRU-K is a variant of the LRU (least recently used policy) which removes
 * the frame from the cache that has not be accessed for the longest. LRU-K
 * uses a slightly different metric by paying attention more specifically
 * to the k-th latest access of the page.
 *
 * The page to be removed is selected using the following conditions:
 * 1. If frame exist with fewer than k accesses, evict the one with the least
 *    recent access.
 * 2. Otherwise, remove the frame that had the least recent k-th access.
 *
 * Further information can be found in the following Wikipedia article:
 * https://en.wikipedia.org/wiki/Page_replacement_algorithm#Least_recently_used
 */
class LRUKReplacer {
    /* History of an individual frame */
    struct LRUFrameHistory {
        LRUFrameHistory() = default;
        LRUFrameHistory(frame_id_t frame_id);

        frame_id_t frame_id { frame_id_t(-1) };

        /* history of accesses */
        std::deque<std::time_t> history;

        /* can the frame evict the page inside of it */
        bool is_evictable { true };
    };

public:
    /**
     * Register a frame to the LRU replacement policy to track accesses
     *
     * NOTE: This function MUST be called for every frame that is meant to be
     * used by the replacer. This is so that the replacer knows which frames
     * are evictable/available at any given time. If not registered the frame
     * will NEVER be evicted as the replacement policy has no knowledge of the
     * frame existing.
     */
    void RegisterFrame(frame_id_t frame_id);

    /**
     * Attempt to evict a frame using the LRU-K replacement policy. If no
     * frame is evictable return std::nullopt.
     */
    std::optional<frame_id_t> EvictFrame();

    void RecordAccess(frame_id_t frame_id);

    void SetEvictable(frame_id_t frame_id, bool evictable);

    std::size_t GetEvictableCount();

private:
    /* keeping track of all of the current frames. */
    std::unordered_map<frame_id_t, LRUFrameHistory> frames_m;

    /* monotonically increasing clock for accesses */
    std::size_t current_timestamp_m { 0 };

    /* maximum length of frame history to account for */
    std::size_t k_m;

    /* the number of evictable frames */
    std::size_t evictable_count_m;
};
