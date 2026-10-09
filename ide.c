// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue.

// IMPORTANT: IDE Interface:
// Control Register:
//    Address 0x3F6 = 0x08 (0000 1RE0): R=reset,
//                    E=0 means "enable interrupt"
// Command Block Registers:
//    Address 0x1F0 = Data Port
//    Address 0x1F1 = Error
//    Address 0x1F2 = Sector Count
//    Address 0x1F3 = LBA low byte
//    Address 0x1F4 = LBA mid byte
//    Address 0x1F5 = LBA hi byte
//    Address 0x1F6 = 1B1D TOP4LBA: B=LBA, D=drive
//    Address 0x1F7 = Command/status
// Status Register (Address 0x1F7):
//      7    6     5     4   3    2     1     0
//    BUSY READY FAULT SEEK DRQ CORR IDDEX ERROR
// Error Register (Address 0x1F1): (check when ERROR==1)
//     7   6   5   4   3    2    1    0
//    BBK UNC MC IDNF MCR ABRT T0NF AMNF
//    BBK = Bad Block
//    UNC = Uncorrectable data error
//    MC = Media Changed
//    IDNF = ID mark Not Found
//    MCR = Media Change Requested
//    ABRT = Command aborted
//    T0NF = Track 0 Not Found
//    AMNF = Address Mark Not Found

static struct spinlock idelock;   // so this lock protects the idequeue
static struct buf *idequeue;

static int havedisk1;   //this variable is set to 1 if theres a second hard disk... in ideinit()
static void idestart(struct buf*);

// NOTE: Wait for IDE disk to become ready and THEN return.
static int
idewait(int checkerr)
{
  // return zero if its not checkerr...
  int r;

  // POLL until the ready bit is set and busy bit is reset.
  while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;
  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
}

// called only once from main, in the beginning
void
ideinit(void)
{
  int i;

  initlock(&idelock, "ide");    // initialize the lock that protect the idequeue
  ioapicenable(IRQ_IDE, ncpu - 1);
  idewait(0);

  // NOTE: Check if disk 1 is present
  outb(0x1f6, 0xe0 | (1<<4));
  for(i=0; i<1000; i++){
    // loop goes from 1 to 1000, the device must respond in that time
    if(inb(0x1f7) != 0){
      havedisk1 = 1;
      break;
    }
  }

  // Switch back to disk 0.
  outb(0x1f6, 0xe0 | (0<<4));
}

// Start the request for b.  Caller must hold idelock.
static void
idestart(struct buf *b)
{
  // NOTE: Starts operation of the buf passed to it

  if(b == 0)
    panic("idestart");
  if(b->blockno >= FSSIZE)
    panic("incorrect blockno");
  int sector_per_block =  BSIZE/SECTOR_SIZE;
  int sector = b->blockno * sector_per_block;
  int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;   // dont know why...
  int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;  // dont know why...

  if (sector_per_block > 7) panic("idestart");

  idewait(0);
  outb(0x3f6, 0);  // generate interrupt
  outb(0x1f2, sector_per_block);  // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));

  // NOTE: iderw has checked the flags... if dirty then its a write operation if not its a read operation
  if(b->flags & B_DIRTY){
    outb(0x1f7, write_cmd);
    outsl(0x1f0, b->data, BSIZE/4);
  } else {
    outb(0x1f7, read_cmd);
  }
}

// NOTE: Interrupt handler to run when the disk raises an interrupt
void
ideintr(void)
{
  struct buf *b;

  // First queued buffer is the active request.
  acquire(&idelock);

  if((b = idequeue) == 0){
    // QUESTION: Why is this here? If the intr was for some operation then the queue wouldnt be empty?
    // Can the disk controller unnecessarily cause hardware interrupts ?
    release(&idelock);
    return;
  }

  idequeue = b->qnext;    // advance in the queue

  // Read data from the disk controller memory into the RAM(buf) if it was a read operation
  if(!(b->flags & B_DIRTY) && idewait(1) >= 0)
    insl(0x1f0, b->data, BSIZE/4);

  // Wake process waiting for this buf.
  b->flags |= B_VALID;    //set valid bit... regardless of whether it was read or write, the valid bit = 1
  b->flags &= ~B_DIRTY;   //reset dirty bit... regardless of whether it was read or write, the dirty bit = 1
  wakeup(b);  // QUESTION: How does this process sleeping on a blocking call work? Whats chan?
              // And how exactly is the requesting process identified ?

  // Start disk on next buf in queue if the queue is not empty.
  if(idequeue != 0)
    idestart(idequeue);

  release(&idelock);
}

//PAGEBREAK!
// Sync buf with disk.
// If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read buf from disk, set B_VALID.
void
iderw(struct buf *b)
{
  // NOTE:The device driver is the bridge between the device controller and the OS,
  // so it has to understand how the OS internally uses the data (struct buf)
  // and it also has to understand how the disk controller works (whatever protocol)
  // This function enqueues the request and starts the request if thats the only request in the queue.

  struct buf **pp;

  if(!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)   // if its not dirty but valid means its an error
    panic("iderw: nothing to do");
  if(b->dev != 0 && !havedisk1)                   // if device no = 1 and we dont have a second disk then error
    panic("iderw: ide disk 1 not present");

  acquire(&idelock);  //DOC:acquire-lock

  // Append b to idequeue.
  b->qnext = 0;
  for(pp=&idequeue; *pp; pp=&(*pp)->qnext)  //DOC:insert-queue
    ;
  *pp = b;

  // Start disk if necessary.
  // NOTE:
  // if b is the only buf in the queue then it means that there was no buf in the queue before, and that the disk is free and thus we need to idestart() the operation
  // if a request is already being serviced then the idestart on our request will be run by the ideintr handler... It calls idestart after wakeup
  if(idequeue == b)
    idestart(b);

  // Wait for request to finish.
  while((b->flags & (B_VALID|B_DIRTY)) != B_VALID){
    sleep(b, &idelock);
  }


  release(&idelock);
}
