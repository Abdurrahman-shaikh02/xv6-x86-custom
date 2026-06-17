// Boot loader.
//
// Part of the boot block, along with bootasm.S, which calls bootmain().
// bootasm.S has put the processor into protected 32-bit mode.
// bootmain() loads an ELF kernel image from the disk starting at
// sector 1 and then jumps to the kernel entry routine.

#include "types.h"
#include "elf.h"
#include "x86.h"
#include "memlayout.h"

#define SECTSIZE  512

void readseg(uchar*, uint, uint);

void
bootmain(void)
{
  struct elfhdr *elf;
  struct proghdr *ph, *eph;
  void (*entry)(void);
  uchar* pa;

  elf = (struct elfhdr*)0x10000;  // scratch space

  // NOTE: Read the following comments... i have made changes

  // Read 1st page off disk
  readseg((uchar*)elf, 4096, 0);
  //readseg read x bytes from the disk at y offset from the 1st sector... (it by default adds a sector offset cuz kernel code starts from sector 1)
  //So the above call is reading the 

  // Is this an ELF executable?
  if(elf->magic != ELF_MAGIC)
    return;  // let bootasm.S handle error

  // Load each program segment (ignores ph flags).
  ph = (struct proghdr*)((uchar*)elf + elf->phoff);
  eph = ph + elf->phnum;
  for(; ph < eph; ph++){
    //go through every entry in the program header table... and read every program accordingly...
    pa = (uchar*)ph->paddr; //the address at which the program wants to be loaded
    readseg(pa, ph->filesz, ph->off); //read the filesz bytes from its location in the elf(given by ph->off)
    if(ph->memsz > ph->filesz)
      //if it wants more memory that the file size it is then concatenate zeros to fill up the extra required memory
      //memsz tells how many bytes of memory it wants... filesz tells how big the program is
      stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  // Call the entry point from the ELF header.
  // Does not return!
  // NOTE: the value of elf->entry was set by the linker... (and will be read by the loader(the code that we are running rn)...)
  // the linker knew that the entry symbol will be at this address when the loading gets complete...
  // and it gives us the logical address(in the memory) of where to jump execution, now that the elf file has been loaded into the memory...
  // Entry is in entry.S
  entry = (void(*)(void))(elf->entry);
  entry();
}

void
waitdisk(void)
{
  // Wait for disk ready.
  while((inb(0x1F7) & 0xC0) != 0x40)
    ;
}

// Read a single sector at offset into dst.
void
readsect(void *dst, uint offset)
{
  // Issue command.
  waitdisk();
  outb(0x1F2, 1);   // count = 1
  outb(0x1F3, offset);
  outb(0x1F4, offset >> 8);
  outb(0x1F5, offset >> 16);
  outb(0x1F6, (offset >> 24) | 0xE0);
  outb(0x1F7, 0x20);  // cmd 0x20 - read sectors

  // Read data.
  waitdisk();
  insl(0x1F0, dst, SECTSIZE/4);  
  // NOTE: Why SECTSIZE/4 and not SECTSIZE ?
  // Well we read a double word(32 bits) at a time...
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked.
void
readseg(uchar* pa, uint count, uint offset)
{
  uchar* epa;

  epa = pa + count;

  // Round down to sector boundary.
  pa -= offset % SECTSIZE;

  // Translate from bytes to sectors; kernel starts at sector 1.
  offset = (offset / SECTSIZE) + 1; // NOTE: why add plus one here ? Better to let the caller call with 512 offset...
  //now offset represents no of sectors to be skipped from start...

  // If this is too slow, we could read lots of sectors at a time.
  // We'd write more to memory than asked, but it doesn't matter --
  // we load in increasing order.
  for(; pa < epa; pa += SECTSIZE, offset++)
    readsect(pa, offset);
}
