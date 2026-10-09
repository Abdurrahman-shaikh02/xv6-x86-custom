#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {      // NOTE: We get this structure's address from the super block... (and we make the struct log below)
  int n;                // total number of valid block numbers in the following array
  int block[LOGSIZE];   // contains the block number where the data block goes...
};                      // this struct is an ON-DISK data structure.
                        // The subsequent blocks on the disk after the header contain the data blocks corresponding to the block numbers in the array.
                        // See supplementary_figures/xv6_fslog.png

struct log {            // NOTE: we get the following data from the super block
  struct spinlock lock; // lock protecting this structure.  
  int start;            // the first log block on the disk... from the super block
  int size;             // total number of log data blocks that are in use out of 30 
  int outstanding;      // how many FS sys calls are currently executing.
  int committing;       // in commit(), please wait until the commit is over.
  int dev;              // the device number to identify the disk containing the file system (ie. fs.img)
  struct logheader lh;  // the IN-MEMORY copy of the logheader from the disk
};
struct log log;

static void recover_from_log(void);
static void commit();

// QUESTION: Why is this not from the main() function, and instead called from forkret()'s first run?
// NOTE: Initializes the struct log.
// Get the super block, and initialize the in-memory struct log from the super block
void
initlog(int dev)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  struct superblock sb;
  initlock(&log.lock, "log");
  readsb(dev, &sb);
  log.start = sb.logstart;    
  log.size = sb.nlog;
  log.dev = dev;
  recover_from_log();
}

// NOTE: Copy committed blocks from log to their home location
// All valid log data blocks to their corresponding locations in the block
static void
install_trans(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1);    // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]);  // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);                 // copy block to dst
    bwrite(dbuf);                                           // write dst to disk
    brelse(lbuf);
    brelse(dbuf);
  } // do this for every valid log data block
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// NOTE: Write in-memory log header to disk.
// This is the true point at which the current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);              // read the block containing the on-disk logheader into a buf
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  // copy the in-memory logheader to the buf
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);    // write the buf to the disk.
  brelse(buf);    // we are done with the buf now.
}

// NOTE: Only called from initlog.
static void
recover_from_log(void)
{
  read_head();
  install_trans();  // if committed, copy from log to disk... Complete the incomplete transaction.
  log.lh.n = 0;     // clear the on-disk logheader.
  write_head();     // clear the log
}

// NOTE: Called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      // if we are currently commiting then we need to wait until the transaction is fully completed (committed) so go to sleep
      sleep(&log, &log.lock);       
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // There is a possibility that this system call (begin_op is called at the beginning of every fs system call)
      // will cause the struct logheader to get full. Thus wait for previous FS syscalls to get over and then committed.
      // How to decide that ? We are assuming that an FS call will only write upto 10 blocks.
      // So go to sleep.
      sleep(&log, &log.lock);
    } else {
      // if neither of the above two problems then ok
      // increment outstanding fields to show that one more fs system call is active.
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// NOTE: Called at the end of each FS system call.
// Commits if this was the last outstanding FS system call.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if(log.committing)
    // if we are committing that means that the last active FS system call is over. How is this getting called again? -> PANIC
    panic("log.committing");
  if(log.outstanding == 0){   
    // if this was the last remaining FS system call, then we want to commit.
    // IMPORTANT: The last FS system call (or last operation) getting finished marks the END of a TRANSACTION.
    // Similarly the outstanding value changing from 0 to 1 marks the START of a TRANSACTION.
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space, and decrementing log.outstanding has decreased the amount of reserved space.
    // So try calling wakeup if some begin_op() was waiting to allow it.
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();   // take data from the log area of the disk and write into the actual data blocks.
    acquire(&log.lock);
    // once the commit is over, acquire spinlock, reset committing and wakeup() any process that might be waiting in begin_op for commit() to get over.
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// NOTE: Copy modified blocks from cache to log. (In log_write we only noted the block number in the in memory struct log)
// Only called from commit().
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1);      // get a log block (basically get a buffer and the meta data of the data block in the log section)
    struct buf *from = bread(log.dev, log.lh.block[tail]);  // cache block (get a pointer to the buf that we want to write to disk)
    memmove(to->data, from->data, BSIZE);                   // copy data to the log block.
    bwrite(to);     // write the log to disk
    brelse(from);   // we are done with the buf, we wrote it to the disk (in the log block not the actual file system yet.)
    brelse(to);     // we are done with the log block as well
  } // do this for every valid block number in the in memory logheader.
}

// NOTE: Only called from end_op().
static void
commit()
{
  if (log.lh.n > 0) { // if the log contains something
    write_log();      // Write modified blocks from buffer cache to log blocks
    write_head();     // Write logheader to disk -- the true commit
    install_trans();  // Now install writes to home locations (copy from log data blocks to actual blocks in the FS)
    log.lh.n = 0;     // reset the in memory logheader because all pending writes are complete now. The TRANSACTION is complete.
    write_head();     // Erase the transaction from the on-disk logheader.
  }
}

// IMPORTANT: 
// Usage: Caller has modified b->data and is done with the buffer.
// Now record the block number and pin in the cache with B_DIRTY.
// Now call log_write to ONLY NOTE DOWN THE BLOCK NUMBER IN THE logheader.
// commit()/write_log() will do the actual disk write.
//
// IMPORTANT:
// log_write() replaces bwrite(); a typical use is:
//    bp = bread(...)
//    modify bp->data[]
//    log_write(bp)
//    brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");       // if you do too many operations in a single transaction -> panic
  if (log.outstanding < 1)
    panic("log_write outside of trans");  

  acquire(&log.lock);
  // iterate over all the valid block numbers in logheader.
  // if the block number already exists use that.
  // if not select the next unused slot in logheader's array of block numbers.
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n)
    log.lh.n++;         // update the value of n in logheader if a new slot was used.
  b->flags |= B_DIRTY;  // prevent eviction from the buffer cache.
  release(&log.lock);
}

