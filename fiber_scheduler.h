#pragma once
#include <threads.h>
#include <stdatomic.h>
#include <stdint.h>

// Compile-time settings.
#define FIBER_SCHEDULER_THREAD_COUNT 1

// Initialization
void fiber_scheduler_init(size_t stack_size);
void fiber_scheduler_free();

// Scheduling
void fiber_schedule(void(*func)(void));
void fiber_schedule_arg(void(*func)(void*), void* arg, size_t arg_size);

// Fiber control
void fiber_yield();
void fiber_finish();

