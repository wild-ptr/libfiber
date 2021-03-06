as create_context.S -o create_context.o
as restore_context.S -o restore_context.o
as restore_context_args.S -o restore_context_args.o
as restore_context_continue.S -o restore_context_continue.o

gcc -m64 -std=gnu11 -g -pthread -D DEBUG create_context.o restore_context.o restore_context_args.o restore_context_continue.o vec.h vec.c sync_primitives.c sync_primitives.h fiber_scheduler.h fiber_scheduler.c fiber_assemblylink.c fifo_queue.c fifo_queue.h -o fibers
