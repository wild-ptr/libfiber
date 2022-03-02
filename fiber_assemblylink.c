#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <emmintrin.h>
#include "fiber_scheduler.h"

FiberScheduler mgr;

void foo_fiber()
{
    printf("\nCalled as a fiber\n");
    fiber_yield(&mgr);
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


    //int* intarg = (int*)arg;
    //for(int i = 0; i < 4; ++i)
    //{
    //    printf("fiberarg:%p : %i\n",&intarg[i], intarg[i]);
    //}
    fiber_yield(&mgr);
}

int main()
{
    fiber_scheduler_init(&mgr, 4096);

    //fiber_schedule(&mgr, foo_fiber);
    int arr[] = {0, 1, 2, 3, 4};

    struct fiber_args fa = { .nums[3] = 5, .a = 'x', .b = 'y', .f = 42.420f };

    fiber_schedule_arg(&mgr, foo_fiber_arg, &fa, sizeof(fa));
    fiber_scheduler_free(&mgr);
    return 0;
}
