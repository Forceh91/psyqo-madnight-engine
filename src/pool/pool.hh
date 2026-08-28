#pragma once

#include <cstdint>

static constexpr int16_t INVALID_POOL_ID = 0xFFFF;

// T = type of object to pool, N = pool size (at least 1)
template<class T, int16_t N>
class Pool {
public:
    Pool() {
        Dump();
    };

    // gives you the INDEX of the next free slot and immediately marks it as used
    int16_t Acquire(void) {
        if (m_nextFreeIx == INVALID_POOL_ID)
            return INVALID_POOL_ID;

        // get the current free ix
        auto id = m_nextFreeIx;

        // set the next free ix to what our current next free ix is pointing at
        m_nextFreeIx = m_freeIxs[id];
        return id;
    }
    
    // frees up this INDEX for use again, you must handle memory cleanup of the entry yourself
    void Free(int16_t ix) {
        if (ix < 0 || ix >= N)
            return;

        // the current free index gets marked as next free for this slot
        // and then we change the current free index to the one we just freeed
        m_freeIxs[ix] = m_nextFreeIx;
        m_nextFreeIx = ix;        
    }

    // gives you a pointer into an internal entries array that you can then begin filling out
    // use `Acquire` first to get a safe index to use, dont just guess
    T* Get(int16_t ix) {
        if (ix < 0 || ix >= N)
            return nullptr;

        return &m_entries[ix];
    }

    // gives you a pointer to all internal entries
    const T* Entries(void) const {
        return m_entries;
    }

    // marks all indexes as free and sets the next free index to 0
    // this does not clear out anything within the internal entries pool
    // thats up to the manager to do
    void Dump(void) {
        auto safeSize = N < 0 ? 1 : N;
        for (int16_t i = 0; i < safeSize; i++)
            m_freeIxs[i] = i + 1;

        m_freeIxs[safeSize - 1] = INVALID_POOL_ID;
        m_nextFreeIx = 0;
    }

    const int16_t size(void) const { return N; }

private:
    int16_t m_freeIxs[N];
    int16_t m_nextFreeIx = 0;
    T m_entries[N];
};
