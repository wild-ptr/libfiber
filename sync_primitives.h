#pragma once
#include <threads.h>
#include <stdatomic.h>

// Fiber-safe spinlock which will not yield to OS.
// This assumes a properly-initialized atomic_flag
void sync_spinlock(atomic_flag* f);

void sync_spinunlock(atomic_flag* f);

// Function-like macros.
#define cond_var_wait_f(cnd_t, mtx_t, f) \
    while(!f()) \
    { \
        cnd_wait(cnd_t, mtx_t); \
    }


#define cond_var_wait_f_args(cnd_t, mtx_t, f, ...) \
    while(!f(__VA_ARGS__)) \
    { \
        cnd_wait(cnd_t, mtx_t); \
    }

