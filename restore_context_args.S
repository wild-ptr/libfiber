.type restore_context_args, @function
.global restore_context_args
restore_context_args:
  # Load new stack pointer.
  movq 8*1(%rdi), %rsp

  # Load preserved registers.
  movq 8*2(%rdi), %rbx
  movq 8*3(%rdi), %rbp
  movq 8*4(%rdi), %r12
  movq 8*5(%rdi), %r13
  movq 8*6(%rdi), %r14
  movq 8*7(%rdi), %r15
    
  #save rdi, will be needed to call.
  movq %rdi, %rax

  # Pass pointer to first function argument.
  movq %rsi, %rdi

  # Invoke the target function.
  callq *(8*0)(%rax)

  # Return. Never called.
  xorl %eax, %eax
  ret
