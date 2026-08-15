# Context switch
#
#   void swtch(struct context **old, struct context *new);
# 
# Save the current registers on the stack, creating
# a struct context, and save its address in *old.
# Switch stacks to new and pop previously-saved registers.

.globl swtch
swtch:
  # NOTE: These are the parameters to the function swtch
  movl 4(%esp), %eax  # pointer to pointer to old context
  movl 8(%esp), %edx  # pointer to new context

  # Save old callee-saved registers
  # IMPORTANT: We are pushing the old context on to the stack. And forming the struct context!
  # The return address (addr after the swtch call) was pushed implicitly.
  # This return address is the eip that we start from when the context switches back to this process.
  # Then we push ebp, ebx, esi, edi. So the stack now looks like :-
  # edi <-------esp
  # esi
  # ebx
  # ebp
  # return addr(eip)
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi
  # Now the struct context has been saved on the stack

  # Switch stacks
  movl %esp, (%eax)   # IMPORTANT: Now esp is the pointer to the struct context; save it in the location which holds the pointer to the old context...
                      # For a process to be switched out its the pcb's context field
                      # And if it is the scheduler context then its the struct cpus scheduler field
  movl %edx, %esp     # Now change sp to the new context which was setup in the same fashion

  # IMPORTANT: So now the stack is :- (new means the new context)
  # (new)edi <-------esp
  # (new)esi
  # (new)ebx
  # (new)ebp
  # (new)return addr(eip)

  # Load new callee-saved registers
  # NOTE: Notice how the order is exactly the reverse of the order in which they were pushed above...
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
