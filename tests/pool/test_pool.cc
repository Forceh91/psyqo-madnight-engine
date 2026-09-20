#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"

#include "../../src/pool/pool.hh"

// simple stand-in for a manager-owned type (like LoadedMeshBin/Billboard)
struct DummyEntry {
    int16_t id = INVALID_POOL_ID;
    uint32_t value = 0;
};

TEST_CASE("fresh pool hands out sequential indices starting at 0") {
    Pool<DummyEntry, 4> pool;

    CHECK(pool.Acquire() == 0);
    CHECK(pool.Acquire() == 1);
    CHECK(pool.Acquire() == 2);
    CHECK(pool.Acquire() == 3);
}

TEST_CASE("acquiring past capacity returns INVALID_POOL_ID and stays exhausted") {
    Pool<DummyEntry, 2> pool;

    CHECK(pool.Acquire() == 0);
    CHECK(pool.Acquire() == 1);

    // pool is now full — repeated calls must keep returning the sentinel,
    // not wrap around or hand out a duplicate/out-of-range index
    CHECK(pool.Acquire() == INVALID_POOL_ID);
    CHECK(pool.Acquire() == INVALID_POOL_ID);
}

TEST_CASE("Get returns nullptr for out-of-range or negative indices") {
    Pool<DummyEntry, 4> pool;

    CHECK(pool.Get(-1) == nullptr);
    CHECK(pool.Get(4) == nullptr);
    CHECK(pool.Get(100) == nullptr);
}

TEST_CASE("Get(INVALID_POOL_ID) returns nullptr, not a stale/garbage entry") {
    Pool<DummyEntry, 4> pool;

    // this is the exact mistake a caller makes if they forget to check
    // Acquire()'s return before calling Get() — e.g. calling Get on a
    // full pool's sentinel result. It must fail safely, not hand back
    // a pointer into entries[-1]/garbage memory.
    CHECK(pool.Get(INVALID_POOL_ID) == nullptr);
}

TEST_CASE("Get returns a usable pointer into the pool's own storage") {
    Pool<DummyEntry, 4> pool;

    auto ix = pool.Acquire();
    auto* entry = pool.Get(ix);
    REQUIRE(entry != nullptr);

    entry->value = 1234;

    // fetching the same index again must see the same write —
    // proves Get points at live pool storage, not a copy
    CHECK(pool.Get(ix)->value == 1234);
}

TEST_CASE("Free pushes the index back onto the free list (LIFO)") {
    Pool<DummyEntry, 3> pool;

    auto a = pool.Acquire(); // 0
    auto b = pool.Acquire(); // 1
    auto c = pool.Acquire(); // 2
    CHECK(a == 0);
    CHECK(b == 1);
    CHECK(c == 2);

    // free the middle one — it should be the very next one handed out,
    // since Free pushes onto the head of the free list
    pool.Free(b);
    CHECK(pool.Acquire() == b);

    // pool is full again now (a, b, c all acquired) — confirm exhaustion
    CHECK(pool.Acquire() == INVALID_POOL_ID);
}

TEST_CASE("Free with an out-of-range index is a no-op, not a crash") {
    Pool<DummyEntry, 2> pool;

    pool.Acquire(); // 0
    pool.Acquire(); // 1

    // pool is full; these should be silently ignored rather than
    // corrupting internal bookkeeping
    pool.Free(-1);
    pool.Free(2);
    pool.Free(999);

    // pool should still correctly report itself as full
    CHECK(pool.Acquire() == INVALID_POOL_ID);
}

TEST_CASE("Dump resets the pool so every slot is acquirable again") {
    Pool<DummyEntry, 3> pool;

    pool.Acquire();
    pool.Acquire();
    pool.Acquire();
    CHECK(pool.Acquire() == INVALID_POOL_ID); // confirm it was actually full

    pool.Dump();

    // after Dump, indices should be handed out from scratch, 0..N-1 again
    CHECK(pool.Acquire() == 0);
    CHECK(pool.Acquire() == 1);
    CHECK(pool.Acquire() == 2);
    CHECK(pool.Acquire() == INVALID_POOL_ID);
}

TEST_CASE("Dump does not touch existing entry data (that's the manager's job)") {
    Pool<DummyEntry, 2> pool;

    auto ix = pool.Acquire();
    pool.Get(ix)->value = 42;

    pool.Dump();

    // the slot's old data should still be sitting there untouched —
    // Pool explicitly does not clear entries on Dump/Free
    CHECK(pool.Get(ix)->value == 42);
}

TEST_CASE("size() reports the compile-time capacity") {
    Pool<DummyEntry, 7> pool;
    CHECK(pool.size() == 7);
}

TEST_CASE("Entries() points at the same storage Get() does") {
    Pool<DummyEntry, 3> pool;

    auto ix = pool.Acquire();
    pool.Get(ix)->value = 77;

    // Entries()[ix] and Get(ix) must be the exact same object
    CHECK(&pool.Entries()[ix] == pool.Get(ix));
    CHECK(pool.Entries()[ix].value == 77);
}

TEST_CASE("pool of size 1 behaves correctly at the edges") {
    Pool<DummyEntry, 1> pool;

    CHECK(pool.Acquire() == 0);
    CHECK(pool.Acquire() == INVALID_POOL_ID);

    pool.Free(0);
    CHECK(pool.Acquire() == 0);
}
