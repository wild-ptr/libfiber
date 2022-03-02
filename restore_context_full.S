.type restore_context, @function
.global restore_context
restore_context:
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

  # Push RIP to stack for RET.
  #pushq %r8

  # direct call. ABI requires the stack to be 16-aligned during call.
  # pushq destroys that.
  call *(8*0)(%rdi)

  # Return.
  xorl %eax, %eax
  ret
