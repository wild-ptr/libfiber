#include <stdio.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>

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
};

void save_context(struct fiber_context* context)
{
    // just to check which one is stack pointer. fiber context should be close by.
    //printf("Pointer to fiber context in save_ctx: %p\n", context);
    // %0 is %%rdi
    // @TODO: check x86 sysv abi stack frame structure
    // This is all in GAS syntax, not intel
    asm volatile(
         //save stack pointer for return.
        "movq %%rsp, 8*1(%0);" // RIP
        "leaq 8(%%rsp), %%r8;" // return address is just above rsp in sysv?
        "movq %%r8, 8*0(%0);" // RSP

        "movq %%rbx, 8*2(%0);"
        "movq %%rbp, 8*3(%0);"
        "movq %%r12, 8*4(%0);"
        "movq %%r13, 8*5(%0);"
        "movq %%r14, 8*6(%0);"
        "movq %%r15, 8*7(%0);"

        //"xorl %%eax, %%eax;"
        //"ret;"

            : "+g"(context) //output
            : //input
            : "%r8"); // clobber

    // here the compiler should autogenerate xorl eax eax + ret
}

void restore_context(struct fiber_context* context)
{
    asm volatile(
        "movq 8*1(%0), %%rsp;" // change base pointer.
        "movq 8*2(%0), %%rbx;"
        "movq 8*3(%0), %%rbp;"
        "movq 8*4(%0), %%r12;"
        "movq 8*5(%0), %%r13;"
        "movq 8*6(%0), %%r14;"
        "movq 8*7(%0), %%r15;"
        "movq 8*0(%0), %%r8;"

        "pushq %%r8;"
        //"xorl %%eax, %%eax;"
        //"ret;"

        :
        : "r"(context)
        : "%r8");
}

__attribute__((section(".text#")))
static unsigned char save_context_code[] =
{
  0x4c, 0x8b, 0x04, 0x24,
  0x4c, 0x89, 0x07,
  0x4c, 0x8d, 0x44, 0x24, 0x08,
  0x4c, 0x89, 0x47, 0x08,
  0x48, 0x89, 0x5f, 0x10,
  0x48, 0x89, 0x6f, 0x18,
  0x4c, 0x89, 0x67, 0x20,
  0x4c, 0x89, 0x6f, 0x28,
  0x4c, 0x89, 0x77, 0x30,
  0x4c, 0x89, 0x7f, 0x38,
  0x31, 0xc0,
  0xc3
};

__attribute__((section(".text#")))
static unsigned char restore_context_code[] =
{
    0x4c, 0x8b, 0x07,
    0x48, 0x8b, 0x67, 0x08,
    0x48, 0x8b, 0x5f, 0x10,
    0x48, 0x8b, 0x6f, 0x18,
    0x4c, 0x8b, 0x67, 0x20,
    0x4c, 0x8b, 0x6f, 0x28,
    0x4c, 0x8b, 0x77, 0x30,
    0x4c, 0x8b, 0x7f, 0x38,
    0x41, 0x50,
    0x31, 0xc0,
    0xc3
};

void (*restore_context_asm)(struct fiber_context*) =
    (void(*)(struct fiber_context*))restore_context_code;

void (*save_context_asm)(struct fiber_context*) =
    (void(*)(struct fiber_context*))save_context_code;

static struct fiber_context ctx = {};
int function()
{
    printf("inside function, saving exec now!\n");
    printf("context saved!\n");
}

// This should not work well
int main()
{
    struct fiber_context context = {};
    printf("context ptr: %p\n", &context);
    save_context(&ctx);
    printf("context rip: %p, rsp: %p\n", ctx.rip, ctx.rsp);
    restore_context(&ctx);
    printf("Restoring previous context!\n");
    return 0;
}

