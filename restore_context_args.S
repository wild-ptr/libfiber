.type restore_context_args, @function
.global restore_context_args
restore_context_args:
  # Should return to the address set with {get, swap}_context.
  #movq 8*0(%rdi), %r8

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
  movq %rdi, %r8

  # Pass pointer to first function argument.
  movq 8*10(%rdi), %rdi

  # direct call. ABI requires the stack to be 16-aligned during call.
  # Normally call pushes shit to stack. We can pad the stack with an useless value and jmp,
  # which will help us later
  pushq %r8
  jmp *(8*0)(%r8)
  //call *(8*0)(%r8)

  # Return.
  xorl %eax, %eax
  ret
