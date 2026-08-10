#include "mmu.h"

  # vectors.S sends all traps here.
.globl alltraps
alltraps:
  # Build trap frame.
# IMPORTANT: Notice the order in which the elements have been pushed... ds, es, fs, gs, then eax,ecx,edx,ebx,oesp,ebp,esi,edi (by the pushal instruction)
# And then the esp (which is nothing but the address of the stack element which contains edi...)
# Now notice that the trap function takes as argument a pointer to a struct trapframe... realise that, that is the value of esp that was pushed
# And also realise that, that address + 4 will give esi value and then ebp and so on...
# Now take a look at the struct trapframe... The order of elements is exactly reverse...
# THAT is what building the trap frame means... build the struct, and pass its address to trap function....
# BUT still the entire trap frame hasnt been built no ? Well it has...
# The remaining values of the struct: trap no, errno, eip, cs, eflags, esp, ss have been pushed before the call to alltraps
# The trapno, errno were pushed manually (see vectors.S) and the remaining 5 were pushed by a hardware mechanism right after the interrupt occured(see nb notes)

  pushl %ds
  pushl %es
  pushl %fs
  pushl %gs
  pushal
  
  # Set up data segments.
  movw $(SEG_KDATA<<3), %ax
  movw %ax, %ds
  movw %ax, %es

  # Call trap(tf), where tf=%esp
  pushl %esp
# NOTE: go to trap(...) in trap.c
  call trap
  addl $4, %esp

  # Return falls through to trapret...
.globl trapret
trapret:
  popal
  popl %gs
  popl %fs
  popl %es
  popl %ds
  addl $0x8, %esp  # trapno and errcode
  iret
