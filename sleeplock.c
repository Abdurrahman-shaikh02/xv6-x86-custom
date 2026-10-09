// Sleeping locks

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"

void
initsleeplock(struct sleeplock *lk, char *name)
{
  // All sleeplock must be initialized before being used
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

// IMPORTANT: One thing to notice is that a sleep lock when acquired doesnt cause interrupts to be disabled...
// Its pretty obvious why; if the thread goes to sleep while disabling the interrupts then thats a deadlock....
void
acquiresleep(struct sleeplock *lk)
{
  acquire(&lk->lk);   // acquire the spinlock protecting the sleeplock
  while (lk->locked) {
    sleep(lk, &lk->lk);   // if the lock is already held by someone, block
  }
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);   // release the spinlock protecting the sleeplock
}

void
releasesleep(struct sleeplock *lk)
{
  acquire(&lk->lk);   // acquire the spinlock protecting the spinlock
  lk->locked = 0;
  lk->pid = 0;
  wakeup(lk);
  release(&lk->lk);   // release the spinlock protecting the sleeplock
}

int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}



