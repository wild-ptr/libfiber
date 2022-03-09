#include "sync_primitives.h"

void sync_spinlock_noyield(atomic_flag* f)
{
    // if test-and-set returns true it means we did not acquire lock. It was still locked.
    // when previous value was false, we have successfuly locked.
    // I think this can be done explicit with acquire ordering on lock and release ordering on clear.
    while(atomic_flag_test_and_set_explicit(f, memory_order_acquire))
    {
        // You spin me baby round and round.
    }
}

void sync_spinunlock(atomic_flag* f)
{
    atomic_flag_clear_explicit(f, memory_order_release);
}

