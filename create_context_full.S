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

  #extension, save other registers.
  movq %rax, 8*8(%rdi)
  movq %rsi, 8*9(%rdi)
  movq %rdi, 8*10(%rdi)
  movq %rcx, 8*11(%rdi)

  movq %r8, 8*12(%rdi)
  movq %r9, 8*13(%rdi)
  movq %r10, 8*14(%rdi)
  movq %r11, 8*15(%rdi)

  movups %xmm0, 8*16+16*0(%rdi)
  movups %xmm1, 8*16+16*1(%rdi)
  movups %xmm2, 8*16+16*2(%rdi)
  movups %xmm3, 8*16+16*3(%rdi)
  movups %xmm4, 8*16+16*4(%rdi)
  movups %xmm5, 8*16+16*5(%rdi)
  movups %xmm6, 8*16+16*6(%rdi)
  movups %xmm7, 8*16+16*7(%rdi)
  movups %xmm8, 8*16+16*8(%rdi)
  movups %xmm9, 8*16+16*9(%rdi)
  movups %xmm10, 8*16+16*10(%rdi)
  movups %xmm11, 8*16+16*11(%rdi)
  movups %xmm12, 8*16+16*12(%rdi)
  movups %xmm13, 8*16+16*13(%rdi)
  movups %xmm14, 8*16+16*14(%rdi)
  movups %xmm15, 8*16+16*15(%rdi)

  #x87 fpu state is still not saved.

  # Return.
  xorl %eax, %eax
  ret
