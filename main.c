#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file
                   // IMPORTANT:: This value was set by the linker... the linker instructions that we gave in kernel.ld set this value
                   // this is the first value that does not belong to the kernels image in the memory
                   // IN xv6, the kernel loads only in one place... not many different places like other modern operating systems

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{ 
  //---------------------------------------------------------------------------------------------------------------
  // IMPORTANT:
  // Initialization happens in two phases.
  // 1. main() calls kinit1() while still using entrypgdir to place just
  // the pages mapped by entrypgdir on free list. 
  // (remember we had mapped the first page and the first kernel page to frame 0 in entry.S when we set up pdt?)
  // 2. main() calls kinit2() with the rest of the physical pages
  // after installing a full page table that maps them on all cores.
  kinit1(end, P2V(4*1024*1024)); // phys page allocator
  // NOTE: Remember that me mapped the first 4mb physical space to 0 to 4mb and KERNBASE to KERNBASE+ 4096
  // so kinit1 links the remaining space (from end (marked by linker see above) till KERNBASE + 4096 which is just end to 4mb in physical mem) as 4kb spaces in a linked list...
  // Now every call to kalloc and kfree will work
  // --------------------------------------------------------------------------------------------------------------
  //
  // IMPORTANT: This function sets up the kernel page table. The entire address space 0 to 4gb is mapped onto the physical addess space
  // as given in supplementary_material/xv6_vm_mappings.png.
  // Note that it only sets up the page directory and the required page tables. It does not yet free them(thats done by kinit2)
  kvmalloc();      // kernel page table
  //---------------------------------------------------------------------------------------------------------------
  mpinit();        // detect other processors
  //---------------------------------------------------------------------------------------------------------------
  lapicinit();     // interrupt controller
  //---------------------------------------------------------------------------------------------------------------
  //
  // IMPORTANT: This function sets up the proper global descriptor table. The one we set earlier was temporary.
  seginit();       // segment descriptors
  //---------------------------------------------------------------------------------------------------------------
  picinit();       // disable pic
  //---------------------------------------------------------------------------------------------------------------
  ioapicinit();    // another interrupt controller
  //---------------------------------------------------------------------------------------------------------------
  consoleinit();   // console hardware
  //---------------------------------------------------------------------------------------------------------------
  uartinit();      // serial port
  //---------------------------------------------------------------------------------------------------------------
  pinit();         // process table
  //---------------------------------------------------------------------------------------------------------------
  tvinit();        // trap vectors
  //---------------------------------------------------------------------------------------------------------------
  binit();         // buffer cache
  //---------------------------------------------------------------------------------------------------------------
  fileinit();      // file table
  //---------------------------------------------------------------------------------------------------------------
  ideinit();       // disk 
  //---------------------------------------------------------------------------------------------------------------
  startothers();   // start other processors
  //---------------------------------------------------------------------------------------------------------------
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers()
  //---------------------------------------------------------------------------------------------------------------
  userinit();      // first user process
  //---------------------------------------------------------------------------------------------------------------
  mpmain();        // finish this processor's setup
  //---------------------------------------------------------------------------------------------------------------
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

// Common CPU setup code.
static void
mpmain(void)
{
  cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
  idtinit();       // load idt register
  xchg(&(mycpu()->started), 1); // tell startothers() we're up
  scheduler();     // start running processes
}

pde_t entrypgdir[];  // For entry.S

// Start the non-boot (AP) processors.
static void
startothers(void)
{
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_start.
  code = P2V(0x7000);
  memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == mycpu())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    stack = kalloc();
    *(void**)(code-4) = stack + KSTACKSIZE;
    *(void(**)(void))(code-8) = mpenter;
    *(int**)(code-12) = (void *) V2P(entrypgdir);

    lapicstartap(c->apicid, V2P(code));

    // wait for cpu to finish mpmain()
    while(c->started == 0)
      ;
  }
}

// The boot page table used in entry.S and entryother.S.
// Page directories (and page tables) must start on page boundaries,
// hence the __aligned__ attribute.
// PTE_PS in a page directory entry enables 4Mbyte pages.

// NOTE: The 0th index and the first page of kernel space are getting filled with a particular value... that is frame number 0
// IMPORTANT: Why are we mapping the first page and the first kern page to the same 0th frame?
// (just a guess : because the 2gb of the ram is made availabe to user programs... the addresses above KERNBASE are for kernel
// and so the user processes cant really access those physical addresses...(if they try to access them then they are mapped to 
// the same 2GB in the memory below KERNBASE ?)
// NO! its just that we need some basic pdt for bootstrapping purpose hence we are doing this... gets changed in kvmalloc()
__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {  //btw 1024 entries * 4MB = 4GB... (remember our limit of ffffffff in the gdt ?)
  // Map VA's [0, 4MB) to PA's [0, 4MB)
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
  [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

