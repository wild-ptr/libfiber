#pragma once
#include <threads.h>
#include <stdatomic.h>
#include <stdint.h>
#include "vec.h"
#define FIBER_SCHEDULER_THREAD_COUNT 4

// fiber scheduler responsible for creating threads, creating fibers to put on threads
// and general scheduling
struct StackManager
{
    char** memoryAreas_vec;
    size_t size;
    size_t stack_size;
    char** free_list_vec;
    atomic_flag free_list_mut;
};

void stack_manager_init(struct StackManager*, size_t stack_size);
void stack_manager_free(struct StackManager*);
char* stack_manager_get_free_stack(struct StackManager*);
void stack_manager_return_stack(struct StackManager*, char* stack);

struct fiber_context
{
    void* rip;
    void* rsp;


    void* rbx;
    void* rbp;
    void* r12;
    void* r13;
    void* r14;
    void* r15;

// argument passing on stack bookkeeping
    void* arg; // heap
    uint32_t arg_size;
    void* rdi; // first arg address.
};


// fiber scheduler responsible for creating threads, creating fibers to put on threads
// and general scheduling
typedef struct FiberScheduler
{
    thrd_t* phys_threads[FIBER_SCHEDULER_THREAD_COUNT];
    struct StackManager stack_mgr;
    struct fiber_context* fibers_vec;
    atomic_flag fibers_mut;
    mtx_t fibers_mtx;
    cnd_t fibers_cv;
} FiberScheduler;

void fiber_scheduler_init(FiberScheduler* sched, size_t stack_size);
void fiber_scheduler_free(FiberScheduler* sp);


// This is actually going to be retarded, but i think it might just work.
// What if we save the execution context just before calling the function,
// and swap the stack then?
// No, the registers in which arguments are passed will fail i think.
// We should manually inject our args pointer into rdi i suppose?
// lets start with no-args version.
void fiber_schedule(FiberScheduler*, void(*func)(void));
void fiber_schedule_arg(FiberScheduler*, void(*func)(void*), void* arg, size_t arg_size);

void fiber_yield(FiberScheduler* s);



