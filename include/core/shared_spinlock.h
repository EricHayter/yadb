#pragma once

#include <atomic>

/*
 * SharedSpinlock
 *
 * Implementation of a shared spin lock intended mainly for frames being passed
 * around by the buffer pool manager. Since each of the frames will be held
 * for only a fraction of time (reading a few tuples or maybe doing a memcpy)
 * a spinlock may provide better performance by staying entirely in usermode
 * and not requiring an operating system trap.
 *
 * This lock should mainly be used on low contention scenarios where critical
 * sections are very short to reduce the amount of CPU cycles being burnt
 * busy waiting.
 */
class SharedSpinlock {
public:
    enum class LockState {
        UNLOCKED,
        EXCLUSIVE,
        SHARED,
    };

    bool try_lock();
    void lock();
    void unlock();

    bool try_lock_shared();
    void lock_shared();
    void unlock_shared();

    /* return the state of the lock; exclusively locked, shared locked, or
     * unlocked */
    LockState State() const;
private:
    /*
     * Lock will be implemented using a 3-state atomic variable with the
     * following encodings of each state of the lock
     * 0 = unlocked, -1 = exclusively locked, >= 1 = shared lock
     *
     * This is done to ensure that not mutexes are required internally.
     */
    std::atomic<int> state_m;
};
