#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <emmintrin.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>

#include <signal.h>

#include "vec.h"

#include "fiber_scheduler.h"
#include "sync_primitives.h"

struct StackManager
{
    char** memoryAreas_vec;
    size_t size;
    size_t stack_size;
    char** free_list_vec;
    mtx_t free_list_mut;
};

struct fiber_context
{
// control registers
    void* rip;
    void* rsp;

// callee-saved registers
    void* rbx;
    void* rbp;
    void* r12;
    void* r13;
    void* r14;
    void* r15;

// bookkeeping
    void* arg;
    uint32_t arg_size;
    bool first_run;
};

// currently_running is 64 aligned since this structure is meant to operate without locks,
// as every thread has its own spot in the array.
// If not aligned to cacheline size, false sharing would occur.
struct FiberScheduler
{
    thrd_t phys_threads[FIBER_SCHEDULER_THREAD_COUNT];
    struct StackManager stack_mgr;
    struct fiber_context* fibers_vec;
    //atomic_flag fibers_mtx;
    mtx_t fibers_mtx;
    cnd_t fibers_cv;
};

// This will sit in global memory, for ease of access from every thread or fiber,
// as it will sit in global and static memory region.
// This structure shall never be visible to the library user.
struct FiberScheduler g_scheduler;

// Every running thread needs its index to access currently_running table.
thread_local uint32_t tl_thread_idx;
thread_local void* tl_currently_running_stack;

#define PREEMPTIVE_STACK_CNT 10

#define is_aligned(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

#define get_ptr_mod(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT))

extern void restore_context(struct fiber_context*);
extern void restore_context_args(struct fiber_context*, void* arg_loc);
extern void restore_context_continue(struct fiber_context*);
extern void create_context(struct fiber_context*);

static void print_fiber_state(struct fiber_context* f)
{
    printf("Fiber context printout:\n");
    printf("rsp: %p\nrip:%p\n", f->rsp, f->rip);
}

static void add_new_mem_area(struct StackManager* mgr)
{
    char* memory = aligned_alloc(16, PREEMPTIVE_STACK_CNT * mgr->stack_size);

#ifdef DEBUG
    memset(memory, 0, PREEMPTIVE_STACK_CNT * mgr->stack_size);
#endif

    vector_add(&mgr->memoryAreas_vec, memory);
    size_t vec_size = vector_size(mgr->memoryAreas_vec);

    // slice the memory area into equal parts
    for(size_t i = 0; i < PREEMPTIVE_STACK_CNT; ++i)
    {
        char* stack = mgr->memoryAreas_vec[vec_size - 1] + (i * mgr->stack_size);
        stack += mgr->stack_size; // stacks grow downward in x86 asm. Point to end of stack.
        vector_add(&mgr->free_list_vec, stack);
    }
}

static void stack_manager_init(struct StackManager* mgr, size_t stack_size)
{
    //atomic_flag_clear(&mgr->free_list_mut);
    mtx_init(&mgr->free_list_mut, mtx_plain);
    mgr->stack_size = stack_size;
    mgr->memoryAreas_vec = vector_create();
    mgr->free_list_vec = vector_create();
    add_new_mem_area(mgr);
}

static void stack_manager_free(struct StackManager* mgr)
{
    size_t size = vector_size(mgr->memoryAreas_vec);
    for(size_t i = 0; i < size; ++i)
    {
        free(mgr->memoryAreas_vec[i]);
    }
    vector_free(mgr->memoryAreas_vec);
    vector_free(mgr->free_list_vec);
}

static char* stack_manager_get_free_stack(struct StackManager* mgr)
{
    mtx_lock(&mgr->free_list_mut);
    size_t num_items = vector_size(mgr->free_list_vec);

    //printf("dumping stack manager free list:\n");
    //for (int i = 0; i < num_items; ++i)
    //{
    //    printf("%i : %p\n", i, mgr->free_list_vec[i]);
    //}

    if(!num_items) // free list ran out of entries. Create new memory area and add stacks to free list.
    {
        printf("ran out of area! Making some space for stacks\n");
        add_new_mem_area(mgr);
        num_items = vector_size(mgr->free_list_vec);
    }

    char* ret = mgr->free_list_vec[num_items - 1];
    printf("vector removal from stackmgr\n");
    vector_remove(&mgr->free_list_vec, num_items - 1);

    mtx_unlock(&mgr->free_list_mut);
    return ret;
}

static void stack_manager_return_stack(struct StackManager* mgr, char* stack)
{
    mtx_lock(&mgr->free_list_mut);
    vector_add(&(mgr->free_list_vec), stack);
    mtx_unlock(&mgr->free_list_mut);
}

#undef PREEMPTIVE_STACK_CNT

static bool fiber_vector_check()
{
    return (vector_size(g_scheduler.fibers_vec) != 0);
}

static void fiber_dispatch(struct fiber_context* fiber)
{
    if(fiber->arg == NULL)
    {
        // Jump, Frodo!
        if(fiber->first_run)
        {
            fiber->first_run = false;
            restore_context(fiber);
        }
        else
        {
            printf("Continuing fiber execution with jmp.\n");
            restore_context_continue(fiber);
        }
    }
    else // we have an argument to pass. Copy it to stack memory, 16-align the memory again.
    {
        // this path is only used for first-run, so remove the flag.
        fiber->first_run = false;

        // fiber->rsp is raw stack we got from manager.
        // calculate where will the args begin on that, and copy to that memory.
        char* stack_p_args = (char*)fiber->rsp - fiber->arg_size;
        memcpy(stack_p_args, fiber->arg, fiber->arg_size);

        free(fiber->arg);
        fiber->arg = NULL;

        // %rdi passes the first (and only) argument, in this case a void*
        void* arg_loc = (char*)fiber->rsp - fiber->arg_size;

        // align %rsp to 16 again after taking arg_size of the stack for ourselves.
        if(is_aligned(stack_p_args, 16))
        {
            fiber->rsp = stack_p_args - 16;
        }
        else
        {
            // decrement by mod 16 to properly align.
            stack_p_args -= get_ptr_mod(stack_p_args, 16);
            fiber->rsp = stack_p_args - 16;
        }

#ifdef DEBUG
        assert(is_aligned(fiber->rsp, 16));
#endif
        restore_context_args(fiber, arg_loc);
    }
}

static void pop_fiber_schedule_next_after_finish()
{
    mtx_lock(&g_scheduler.fibers_mtx);
    size_t size = vector_size(g_scheduler.fibers_vec);

    while(size == 0)
    {
        printf("waiting for fibers to jump to, going to sleep.\n");
        cond_var_wait_f(&g_scheduler.fibers_cv, &g_scheduler.fibers_mtx, fiber_vector_check);
        size = vector_size(g_scheduler.fibers_vec); // Size needs to be recalculated
    }

    struct fiber_context fiber = g_scheduler.fibers_vec[size - 1];
    vector_remove(&g_scheduler.fibers_vec, size - 1);

    mtx_unlock(&g_scheduler.fibers_mtx);

    fiber_dispatch(&fiber);
}

static void pop_fiber_schedule_next_after_yield()
{
    // no lock, it was acquired in yield.
    // if entering from yield, size is at least 1, since we just added one context.

    size_t size = vector_size(g_scheduler.fibers_vec);
    struct fiber_context fiber = g_scheduler.fibers_vec[size - 1];
    vector_remove(&g_scheduler.fibers_vec, size - 1);
    mtx_unlock(&g_scheduler.fibers_mtx);

    // Point to beginning of the stack - this information is saved in case the fiber finishes.
    // Then this pointer will be added to free-list.
    tl_currently_running_stack = fiber.arg + fiber.arg_size;

    fiber_dispatch(&fiber);
}

void fiber_yield()
{
    mtx_lock(&g_scheduler.fibers_mtx);

    struct fiber_context f = {0};
    create_context(&f);
    //f.rip = __builtin_return_address(0);
    f.rip = &&restore_callee_saved_regs;

    // this is very slow
    vector_insert(&g_scheduler.fibers_vec, 0, f);

    pop_fiber_schedule_next_after_yield();

restore_callee_saved_regs:;
    // after rescheduling we should end here. fiber_yield should restore callee-saved regs
    // and return.
}

void fiber_finish()
{
    // Permanently remove stack from list. Make it lost from history, but give back stack to free list.
    printf("This fiber is done!\n");
    //stack_manager_return_stack(&g_scheduler.stack_mgr, tl_currently_running_stack);
    pop_fiber_schedule_next_after_finish();
}

static void thread_initialize(int* idx)
{
    tl_thread_idx = *idx;
    tl_currently_running_stack = NULL;

    // Blocking signals from OS.
    sigset_t mask;
    sigfillset(&mask);
    assert(pthread_sigmask(SIG_BLOCK, &mask, NULL) == 0 && "failed to block signals");

    pop_fiber_schedule_next_after_finish();
}

void fiber_scheduler_init(size_t stack_size)
{
    mtx_init(&g_scheduler.fibers_mtx, mtx_plain);
    //atomic_flag_clear(&g_scheduler.fibers_mtx);
    cnd_init(&g_scheduler.fibers_cv);
    g_scheduler.fibers_vec = vector_create();

    stack_manager_init(&g_scheduler.stack_mgr, stack_size);

    printf("Initializing fibre scheduler, creating physical threads...\n");
    for(int i = 0; i < FIBER_SCHEDULER_THREAD_COUNT; ++i)
    {
        thrd_create(&g_scheduler.phys_threads[i], thread_initialize, &i);
    }

    printf("Threads created!\n");
}

void fiber_schedule(void(*func)(void))
{
    // do this pre fibers-lock to minimize contention
    char* fiber_stack = stack_manager_get_free_stack(&g_scheduler.stack_mgr);
    struct fiber_context fiber = {
        .rip = func,
        .rsp = fiber_stack,
        .arg = NULL,
        .first_run = true
    };

    mtx_lock(&g_scheduler.fibers_mtx);
    vector_add(&g_scheduler.fibers_vec, fiber);
    cnd_signal(&g_scheduler.fibers_cv); // notify one of the waiting threads that it can run the job.
    mtx_unlock(&g_scheduler.fibers_mtx);
}

void fiber_schedule_arg(void(*func)(void*), void* arg, size_t arg_size)
{
    printf("-------------- Scheduling arg function.\n");
    // we can copy pre-lock to minimize lock contention.
    char* fiber_stack = stack_manager_get_free_stack(&g_scheduler.stack_mgr);
    struct fiber_context fiber = {
          .rip = func,
          .rsp = fiber_stack,
          .arg_size = arg_size,
          .first_run = true
    };

    fiber.arg = malloc(arg_size);
    memcpy(fiber.arg, arg, arg_size);

    mtx_lock(&g_scheduler.fibers_mtx);
    vector_add(&g_scheduler.fibers_vec, fiber);
    cnd_signal(&g_scheduler.fibers_cv); // notify one of the threads that it can run the job.
    mtx_unlock(&g_scheduler.fibers_mtx);
}


// Right now it will stay in infinite loop, we will need an atomic to denote finishing.
// Its ok for now.
void fiber_scheduler_free()
{
    printf("scheduler_free: Waiting for threads to join\n");
    for(int i = 0; i < FIBER_SCHEDULER_THREAD_COUNT; ++i)
    {
        thrd_join(g_scheduler.phys_threads[i], NULL);
    }
    printf("threads joined!");

    vector_free(g_scheduler.fibers_vec);
    stack_manager_free(&g_scheduler.stack_mgr);
}
