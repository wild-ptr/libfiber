#pragma once
#include <threads.h>
#include <stdatomic.h>
#include <stdint.h>
#include "vec.h"
#define FIBER_SCHEDULER_THREAD_COUNT 4

void fiber_scheduler_init(size_t stack_size);
void fiber_scheduler_free();


void fiber_schedule(void(*func)(void));
void fiber_schedule_arg(void(*func)(void*), void* arg, size_t arg_size);
void fiber_yield();
void fiber_finish();

