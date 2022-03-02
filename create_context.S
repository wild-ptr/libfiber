.type create_context, @function
.global create_context
create_context:
  # Save the return address and stack pointer.
  movq (%rsp), %r8
  movq %r8, 8*0(%rdi) # RIP
  leaq 8(%rsp), %r8
  movq %r8, 8*1(%rdi) # RSP

  # Save preserved registers.
  movq %rbx, 8*2(%rdi)
  movq %rbp, 8*3(%rdi)
  movq %r12, 8*4(%rdi)
  movq %r13, 8*5(%rdi)
  movq %r14, 8*6(%rdi)
  movq %r15, 8*7(%rdi)

  # Return.
  xorl %eax, %eax
  ret
