#include "core/shared_spinlock.h"

#include <cassert>

#include <atomic>
#include <string>

bool SharedSpinlock::try_lock()
{
    int32_t expected = 0;
    return state_m.compare_exchange_strong(expected, -1,
        std::memory_order_acquire,
        std::memory_order_relaxed);
}

void SharedSpinlock::lock()
{
    int32_t expected = 0;
    while (!state_m.compare_exchange_weak(expected, -1,
        std::memory_order_acquire,
        std::memory_order_relaxed)) {
        expected = 0;
    }
}

void SharedSpinlock::unlock()
{
    state_m.store(0, std::memory_order_release);
}

bool SharedSpinlock::try_lock_shared()
{
    int32_t expected = state_m.load(std::memory_order_relaxed);

    while (expected >= 0) {
        if (state_m.compare_exchange_weak(expected, expected + 1,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            return true;
        }
    }

    return false;
}

void SharedSpinlock::lock_shared()
{
    int32_t expected = state_m.load(std::memory_order_relaxed);

    while (true) {
        while (expected < 0) {
            expected = state_m.load(std::memory_order_relaxed);
        }

        if (state_m.compare_exchange_weak(expected, expected + 1,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            return;
        }
    }
}

void SharedSpinlock::unlock_shared()
{
    state_m.fetch_sub(1, std::memory_order_release);
}

SharedSpinlock::LockState SharedSpinlock::State() const
{
    int lock_state = state_m.load(std::memory_order_acquire);
    switch (lock_state) {
    case -1:
        return LockState::EXCLUSIVE;
    case 0:
        return LockState::UNLOCKED;
    default: {
        assert(lock_state > 0 && ("Invalid lock state: " + std::to_string(lock_state) + "\n").c_str());
        return LockState::SHARED;
             }
    }
}
