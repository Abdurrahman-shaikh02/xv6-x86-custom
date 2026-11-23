// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
// IMPORTANT: Why this split?
// This early,
// we need memory (for example to set up the kernel pagetable so that we are able to access the whole physical address space)
// but for those we need pages... -> BOOTSTRAP problemo -- fixed by making available the first 4mb
// But now we need other CPUS to start up... they start in real mode, and they use entrypagdir which only maps the low 4mb
// freeing the rest of the memory right now means that the list being lifo it will give a high memory page when they call kalloc
// but entrypgdir doesnt map the higher pages. so FAULT!
// So : use the first 4 mb to get the other cpus running, and ONLY THEN free the remaining memory (THATS WHY KINIT2 must come after startothers)
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;    // IMPORTANT: This early we do not have others cpus running, thus struct cpus havent been setup. So cant use locks yet.
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);   
  // NOTE: get the first address of the first "full" page in the given range
  // might also round down vend... but honestly if you see the loops break condition below... you'll realise its not needed
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);
  
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v; // cooooooooooool...
  // NOTE: The free page, itself contains a struct run at the beginning which is basically linked list node... which points to the next free page
  r->next = kmem.freelist;  //kmem.freelist is the head of the list...
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

