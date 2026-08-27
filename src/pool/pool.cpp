#include "pool.hh"

template<class T, int16_t size>
int16_t Pool<T, size>::Acquire(void) {
    if (m_nextFreeIx == INVALID_POOL_ID)
        return INVALID_POOL_ID;

    // get the current free ix
    auto id = m_nextFreeIx;

    // set the next free ix to what our current next free ix is pointing at
    m_nextFreeIx = m_freeIxs[id];
    return id;
}

template<class T, int16_t size>
T* Pool<T, size>::Get(int16_t ix) {
    if (ix < 0 || ix >= size)
        return nullptr;

    return &m_entries[ix];
}

template<class T, int16_t size>
void Pool<T, size>::Free(int16_t ix) {
    if (ix < 0 || ix >= size)
        return;

    // the current free index gets marked as next free for this slot
    // and then we change the current free index to the one we just freeed
    m_freeIxs[ix] = m_nextFreeIx;
    m_nextFreeIx = ix;
}
