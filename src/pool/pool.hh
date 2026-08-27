#pragma once

#include <cstdint>

static constexpr int16_t INVALID_POOL_ID = 0xFFFF;

template<class T, int16_t size>
class Pool {
public:
    Pool() {
        Dump();
    };

    // gives you the INDEX of the next free slot and immediately marks it as used
    int16_t Acquire(void);
    // frees up this INDEX for use again, you must handle memory cleanup of the entry yourself
    void Free(int16_t ix);
    // gives you a pointer into an internal entries array that you can then begin filling out
    // use `Acquire` first to get a safe index to use, dont just guess
    T* Get(int16_t ix);

private:
    void Dump(void) {
        auto safeSize = size < 0 ? 1 : size;
        for (int16_t i = 0; i < safeSize; i++)
            m_freeIxs[i] = i + 1;

        m_freeIxs[safeSize - 1] = INVALID_POOL_ID;
        m_nextFreeIx = 0;
    }

    int16_t m_freeIxs[size];
    int16_t m_nextFreeIx = 0;
    T m_entries[size];
};
