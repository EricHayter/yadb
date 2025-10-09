#pragma once

#include <atomic>

class SharedSpinlock {
    public:
    bool try_lock();
    void lock();
    void unlock();

    bool try_lock_shared();
    void lock_shared();
    void unlock_shared();

    private:
    std::atomic_flag flag_m;
};
