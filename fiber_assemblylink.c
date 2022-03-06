#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <emmintrin.h>
#include <unistd.h>
#include "fiber_scheduler.h"


void foo_fiber()
{
    printf("\nCalled as a fiber\n");
    fiber_yield();
}

// Lets try something more complex
struct fiber_args
{
    int nums[5];
    char a,b,c;
    float f;
};


void foo_fiber_arg(void* arg)
{
    printf("Fiber jump complete, CPU switched context. Args address: %p\n", arg);
    struct fiber_args* fa = arg;
    printf("Complex structure printout from fiber, param pass test:\n");
    printf("nums[3]: %i, nums[0]: %i, a: %c, b: %c, f:%f\n", fa->nums[3], fa->nums[0], fa->a, fa->b, fa->f);
    printf("Yielding! Should reschedule the same thread.\n");

    fiber_yield();

    printf("Rescheduled after yielding! in foo_fiber_arg!\n");
    fiber_finish();
}

void foo_fiber_simple()
{
    printf("explosion przez blutacza\n");
    fiber_yield();
}

int main()
{
    fiber_scheduler_init(4096);

    //fiber_schedule(&mgr, foo_fiber);
    int arr[] = {0, 1, 2, 3, 4};

    struct fiber_args fa = { .nums[3] = 5, .a = 'x', .b = 'y', .f = 42.420f };

    size_t i = 0;
    while(true)
    {
        usleep(400 * 1000);
        ++i;
        fa.f = 69.0f + i;
        fiber_schedule_arg(foo_fiber_arg, &fa, sizeof(fa));
    }
    fiber_scheduler_free();
    return 0;
}
