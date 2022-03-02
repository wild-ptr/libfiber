#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <emmintrin.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <assert.h>

#include "fiber_scheduler.h"
#include "sync_primitives.h"

#define PREEMPTIVE_STACK_CNT 10

#define is_aligned(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

#define get_ptr_mod(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT))

extern void restore_context(struct fiber_context*);
extern void restore_context_args(struct fiber_context*);
extern void create_context(struct fiber_context*);

static void print_fiber_state(struct fiber_context* f)
{
    printf("Fiber context printout:\n");
    printf("rsp: %p\nrdi: %p\n", f->rsp, f->rdi);
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

void stack_manager_init(struct StackManager* mgr, size_t stack_size)
{
    atomic_flag_clear(&mgr->free_list_mut);
    mgr->stack_size = stack_size;
    mgr->memoryAreas_vec = vector_create();
    mgr->free_list_vec = vector_create();
    add_new_mem_area(mgr);
}

void stack_manager_free(struct StackManager* mgr)
{
    size_t size = vector_size(mgr->memoryAreas_vec);
    for(size_t i = 0; i < size; ++i)
    {
        free(mgr->memoryAreas_vec[i]);
    }
    vector_free(mgr->memoryAreas_vec);
    vector_free(mgr->free_list_vec);
}

char* stack_manager_get_free_stack(struct StackManager* mgr)
{
    sync_spinlock(&mgr->free_list_mut);
    size_t num_items = vector_size(mgr->free_list_vec);

    printf("dumping stack manager free list:\n");
    for (int i = 0; i < num_items; ++i)
    {
        printf("%i : %p\n", i, mgr->free_list_vec[i]);
    }

    if(!num_items) // free list ran out of entries. Create new memory area and add stacks to free list.
    {
        printf("ran out of area! Making some space for stacks\n");
        add_new_mem_area(mgr);
        num_items = vector_size(&mgr->free_list_vec);
    }

    char* ret = mgr->free_list_vec[num_items - 1];
    vector_remove(&mgr->free_list_vec, num_items - 1);

    sync_spinunlock(&mgr->free_list_mut);
    return ret;
}

void stack_manager_return_stack(struct StackManager* mgr, char* stack)
{
    sync_spinlock(&mgr->free_list_mut);
    vector_add(&(mgr->free_list_vec), stack);
    sync_spinunlock(&mgr->free_list_mut);
}

#undef PREEMPTIVE_STACK_CNT

bool fiber_vector_check(FiberScheduler* s)
{
    return (vector_size(s->fibers_vec) != 0);
}

void fiber_yield(FiberScheduler* s)
{
    mtx_lock(&s->fibers_mtx);

    size_t size = vector_size(s->fibers_vec);
    if(size == 0)
    {
        printf("waiting for fibers to jump to, going to sleep.\n");
        cond_var_wait_f_args(&s->fibers_cv, &s->fibers_mtx, fiber_vector_check, s);
        size = vector_size(s->fibers_vec); // Size needs to be recalculated
    }
    printf("Woken up! Acquiring context at idx: %lu.\n", size);

    // Pop context from vector, unlock mutex
    // fiber context could hold information about whether there are args or not,
    // and if so, pointer to arg memory.
    struct fiber_context fiber = s->fibers_vec[size - 1];
    vector_remove(&s->fibers_vec, size - 1);
    mtx_unlock(&s->fibers_mtx);

    if(fiber.arg == NULL)
    {
        // Jump, Frodo!
        restore_context(&fiber);
    }
    else // we have an argument to pass. Copy it to stack memory, 16-align the memory again.
    {
        char* stack_p_args = (char*)fiber.rsp - fiber.arg_size;

        memcpy(stack_p_args, fiber.arg, fiber.arg_size);

        // %rdi passes the first (and only) argument, in this case a void*
        fiber.rdi = (char*)fiber.rsp - fiber.arg_size;

        // align %rsp to 16 again after taking arg_size of the stack for ourselves.
        if(is_aligned(stack_p_args, 16))
        {
            fiber.rsp = stack_p_args;
        }
        else
        {
            // decrement by mod 16 to properly align.
            stack_p_args -= get_ptr_mod(stack_p_args, 16);
            fiber.rsp = stack_p_args;
        }

        // This only needs to be done once, during first fiber run.
        // The fiber can be treated as no-arg fiber afterwards, so argument memory can be freed.
        free(fiber.arg);
        fiber.arg = NULL;

#ifdef DEBUG
        assert(is_aligned(stack_p_args, 16));
        print_fiber_state(&fiber);
#endif
        restore_context_args(&fiber);
    }

    // After returning to yield, the fiber needs to be rescheduled in the future.
    // IF it yielded. We could also never return here... Well... actually i think we could use
    // full fiber state on the fiber stack.
    // @TODO: Change queueing from FIFO to LIFO as otherwise we might have starvation.
    // @TODO: Actually write a decent scheduler for this shit instead of LIFO queue. Someday.
    mtx_lock(&s->fibers_mtx);
    vector_add(&s->fibers_vec, fiber);
    mtx_unlock(&s->fibers_mtx);
}

void fiber_scheduler_init(FiberScheduler* sched, size_t stack_size)
{
    mtx_init(&sched->fibers_mtx, mtx_plain);
    cnd_init(&sched->fibers_cv);
    sched->fibers_vec = vector_create();
    stack_manager_init(&sched->stack_mgr, stack_size);

    printf("Initializing fibre scheduler, creating physical threads...\n");
    for(int i = 0; i < FIBER_SCHEDULER_THREAD_COUNT; ++i)
    {
        thrd_create(&sched->phys_threads[i], fiber_yield, sched);
    }

    printf("Threads created!\n");
}

void fiber_schedule(FiberScheduler* sp, void(*func)(void))
{
    // do this pre fibers-lock to minimize contention
    char* fiber_stack = stack_manager_get_free_stack(&sp->stack_mgr);
    struct fiber_context fiber = { .rip = func, .rsp = fiber_stack, .arg = NULL };

    mtx_lock(&sp->fibers_mtx);
    vector_add(&sp->fibers_vec, fiber);
    cnd_signal(&sp->fibers_cv); // notify one of the waiting threads that it can run the job.
    mtx_unlock(&sp->fibers_mtx);
}

void fiber_schedule_arg(FiberScheduler* sp, void(*func)(void*), void* arg, size_t arg_size)
{
    // we can copy pre-lock to minimize lock contention.
    char* fiber_stack = stack_manager_get_free_stack(&sp->stack_mgr);
    struct fiber_context fiber = { .rip = func, .rsp = fiber_stack, .arg_size = arg_size };
    fiber.arg = malloc(arg_size);
    memcpy(fiber.arg, arg, arg_size);

    mtx_lock(&sp->fibers_mtx);
    vector_add(&sp->fibers_vec, fiber);
    cnd_signal(&sp->fibers_cv); // notify one of the threads that it can run the job.
    mtx_unlock(&sp->fibers_mtx);
}


// Right now it will stay in infinite loop, we will need an atomic to denote finishing.
// Its ok for now.
void fiber_scheduler_free(FiberScheduler* sp)
{
    printf("scheduler_free: Waiting for threads to join\n");
    for(int i = 0; i < FIBER_SCHEDULER_THREAD_COUNT; ++i)
    {
        thrd_join(sp->phys_threads[i], NULL);
    }
    printf("threads joined!");

    vector_free(sp->fibers_vec);
    stack_manager_free(&sp->stack_mgr);
}
