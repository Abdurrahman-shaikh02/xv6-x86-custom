// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID

  struct context *scheduler;   // NOTE:: swtch() here to enter scheduler.
                               // This pointer doesnt constantly hold the context.
                               // It only holds the context when the scheduler is swtiched out.

  struct taskstate ts;         // Used by x86 to find stack for interrupt (the per process kernel stack)
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// NOTE: 
// PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.  
// WHY: But this is not a function call hello ?! WHAT ?!
// We are talking about the function call to swtch(..) here.

// Contexts are stored at the bottom of the stack they
// describe; the stack pointer(btw we are in the per process kernel stack so we are talking about that sp) is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly (but the call to swtch saves the return address(which is the eip) implicitly),
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state - Process Control Block
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process. Obviously allocated during process creation
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
                               
  struct context *context;     // NOTE: swtch() here to run process
                               // It doesnt always hold the current context.
                               // It only holds the context when the process is switched out.

  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
