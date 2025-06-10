#pragma once

#include "common/type_definitions.h"
#include <ctime>
#include <deque>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

/* ============================================================================
 * LRU-K Replacement Policy
 * ============================================================================
 *
 * The LRU-K Replacement Policy
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
    /*! \brief Struct to represent a frame inside of the page buffer. */
    struct LRUFrame {
        LRUFrame() = default;
        LRUFrame(frame_id_t frame_id);
        frame_id_t frame_id{ frame_id_t(-1) };
        std::deque<std::time_t> history; /// history of accesses
        bool is_evictable { true };
    };

public:
    /*!
     * \brief begin tracking a given frame
     *
     * This function must be called for every frame that is meant to be used
     * by the replacer. This is so that the replacer knows which frames
     * are evictable/available at any given time.
     *
     */
    void TrackFrame(frame_id_t frame_id);

    /*! \brief Find a frame (if any) to evict using the LRU-K policy
     * \returns The frame id of the frame to remove
     */
    std::optional<frame_id_t> EvictFrame();

    /*! \brief Record an access to a given frame
     * \param frame_id frame to record access to
     */
    void RecordAccess(frame_id_t frame_id);

    void Remove(frame_id_t frame_id);

    /*! \brief Set the evictable status on a frame
     * \param frame_id the frame to change the evictable status of
     * \param evictable the new evictable status for the frame
     */
    void SetEvictable(frame_id_t frame_id, bool evictable);

    /*! \brief Get the number of evictable frames
     * \returns The amount of frames that are evictable
     */
    std::size_t GetEvictableCount();

private:
    /// keeping track of all of the current frames.
    std::unordered_map<frame_id_t, LRUFrame> frames_m;

    std::size_t current_timestamp_m { 0 }; /// Timestamp for accesses
    std::size_t k_m; /// Length of history to compare to
    std::size_t evictable_count_m { 0 }; /// Amount of evictable frames
};
