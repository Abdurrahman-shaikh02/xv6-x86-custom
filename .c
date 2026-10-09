[1mdiff --git a/bio.c b/bio.c[m
[1mindex a45ff3e..48a1fed 100644[m
[1m--- a/bio.c[m
[1m+++ b/bio.c[m
[36m@@ -27,14 +27,16 @@[m
 #include "buf.h"[m
 [m
 struct {[m
[31m-  struct spinlock lock;[m
[32m+[m[32m  struct spinlock lock;   // protecting the bcache[m
   struct buf buf[NBUF];[m
 [m
   // Linked list of all buffers, through prev/next.[m
   // head.next is most recently used.[m
[32m+[m[32m  // head.tail is the least recently used.[m
   struct buf head;[m
 } bcache;[m
 [m
[32m+[m[32m// called once in main[m
 void[m
 binit(void)[m
 {[m
[36m@@ -43,21 +45,21 @@[m [mbinit(void)[m
   initlock(&bcache.lock, "bcache");[m
 [m
 //PAGEBREAK![m
[31m-  // Create linked list of buffers[m
[32m+[m[32m  // Create a CIRCULAR DOUBLY LINKED list of buffers[m
   bcache.head.prev = &bcache.head;[m
   bcache.head.next = &bcache.head;[m
   for(b = bcache.buf; b < bcache.buf+NBUF; b++){[m
     b->next = bcache.head.next;[m
     b->prev = &bcache.head;[m
[31m-    initsleeplock(&b->lock, "buffer");[m
[32m+[m[32m    initsleeplock(&b->lock, "buffer");    // also initialize the per buf lock[m
     bcache.head.next->prev = b;[m
     bcache.head.next = b;[m
   }[m
 }[m
 [m
[31m-// Look through buffer cache for block on device dev.[m
[32m+[m[32m// NOTE: Look through buffer cache for block on device dev.[m
 // If not found, allocate a buffer.[m
[31m-// In either case, return locked buffer.[m
[32m+[m[32m// In either case, return LOCKED buffer.[m
 static struct buf*[m
 bget(uint dev, uint blockno)[m
 {[m
[36m@@ -65,7 +67,7 @@[m [mbget(uint dev, uint blockno)[m
 [m
   acquire(&bcache.lock);[m
 [m
[31m-  // Is the block already cached?[m
[32m+[m[32m  // NOTE: Is the block already cached? If yes, increment refcnt and return that buf[m
   for(b = bcache.head.next; b != &bcache.head; b = b->next){[m
     if(b->dev == dev && b->blockno == blockno){[m
       b->refcnt++;[m
[36m@@ -75,7 +77,7 @@[m [mbget(uint dev, uint blockno)[m
     }[m
   }[m
 [m
[31m-  // Not cached; recycle an unused buffer.[m
[32m+[m[32m  // NOTE: Not cached; recycle an unused buffer ie refcnt = 0 AND B_DIRTY = 0 FROM THE TAIL of the list[m
   // Even if refcnt==0, B_DIRTY indicates a buffer is in use[m
   // because log.c has modified it but not yet committed it.[m
   for(b = bcache.head.prev; b != &bcache.head; b = b->prev){[m
[36m@@ -92,30 +94,32 @@[m [mbget(uint dev, uint blockno)[m
   panic("bget: no buffers");[m
 }[m
 [m
[31m-// Return a locked buf with the contents of the indicated block.[m
[32m+[m[32m// Return a LOCKED buf with the contents of the indicated block.[m
 struct buf*[m
 bread(uint dev, uint blockno)[m
 {[m
   struct buf *b;[m
 [m
   b = bget(dev, blockno);[m
[31m-  if((b->flags & B_VALID) == 0) {[m
[32m+[m[32m  if((b->flags & B_VALID) == 0) {     // If its invalid that means it was an empty buf and we need to read it in from the disk[m
     iderw(b);[m
   }[m
   return b;[m
 }[m
 [m
[31m-// Write b's contents to disk.  Must be locked.[m
[32m+[m[32m// Write b's contents to disk.  Must be LOCKED.[m
 void[m
 bwrite(struct buf *b)[m
 {[m
   if(!holdingsleep(&b->lock))[m
     panic("bwrite");[m
[31m-  b->flags |= B_DIRTY;[m
[32m+[m[32m  b->flags |= B_DIRTY;    // set it dirty so idewr() knows that its a write operation[m
   iderw(b);[m
 }[m
 [m
[31m-// Release a locked buffer.[m
[32m+[m
[32m+[m[32m// NOTE: Called when you are done using the buf and want to release it[m
[32m+[m[32m// Release a LOCKED buffer.[m
 // Move to the head of the MRU list.[m
 void[m
 brelse(struct buf *b)[m
[1mdiff --git a/buf.h b/buf.h[m
[1mindex 3266495..71e6549 100644[m
[1m--- a/buf.h[m
[1m+++ b/buf.h[m
[36m@@ -1,14 +1,16 @@[m
 struct buf {[m
   int flags;[m
[31m-  uint dev;[m
[31m-  uint blockno;[m
[31m-  struct sleeplock lock;[m
[32m+[m[32m  uint dev;     //identify which disk has this data been read from[m
[32m+[m[32m  uint blockno;[m[41m [m
[32m+[m[32m  struct sleeplock lock;    //every buf has a lock[m
   uint refcnt;[m
   struct buf *prev; // LRU cache list[m
   struct buf *next;[m
[31m-  struct buf *qnext; // disk queue[m
[32m+[m[32m  struct buf *qnext; // disk queue (this is used when it has to be queued in the idequeue... it forms a linked list.)[m
   uchar data[BSIZE];[m
 };[m
[32m+[m
[32m+[m[32m// NOTE: the following flags tell iderw what the operation to perform is...[m
 #define B_VALID 0x2  // buffer has been read from disk[m
 #define B_DIRTY 0x4  // buffer needs to be written to disk[m
 [m
[1mdiff --git a/file.h b/file.h[m
[1mindex 0990c82..964f95b 100644[m
[1m--- a/file.h[m
[1m+++ b/file.h[m
[36m@@ -9,15 +9,16 @@[m [mstruct file {[m
 };[m
 [m
 [m
[31m-// in-memory copy of an inode[m
[32m+[m[32m// NOTE: IN-MEMORY copy of an inode[m
[32m+[m[32m// The on-disk inode struct is different... its called struct dinode.[m
 struct inode {[m
[31m-  uint dev;           // Device number[m
[31m-  uint inum;          // Inode number[m
[31m-  int ref;            // Reference count[m
[31m-  struct sleeplock lock; // protects everything below here[m
[31m-  int valid;          // inode has been read from disk?[m
[32m+[m[32m  uint dev;               // Device number[m
[32m+[m[32m  uint inum;              // Inode number[m
[32m+[m[32m  int ref;                // Reference count[m
[32m+[m[32m  struct sleeplock lock;  // protects everything below here. QUESTION: Why use a sleeplock here? and not a spinlock?[m
[32m+[m[32m  int valid;              // inode has been read from disk?[m
 [m
[31m-  short type;         // copy of disk inode[m
[32m+[m[32m  short type;             // copy of disk inode[m
   short major;[m
   short minor;[m
   short nlink;[m
[1mdiff --git a/fs.c b/fs.c[m
[1mindex f77275f..2a5c81a 100644[m
[1m--- a/fs.c[m
[1m+++ b/fs.c[m
[36m@@ -27,7 +27,7 @@[m [mstatic void itrunc(struct inode*);[m
 // only one device[m
 struct superblock sb; [m
 [m
[31m-// Read the super block.[m
[32m+[m[32m// Read the super block into the global struct superblock sp.[m
 void[m
 readsb(int dev, struct superblock *sb)[m
 {[m
[36m@@ -38,21 +38,24 @@[m [mreadsb(int dev, struct superblock *sb)[m
   brelse(bp);[m
 }[m
 [m
[31m-// Zero a block.[m
[32m+[m[32m//------------------------------------------------------------------------------------------------------------------------------------------------[m
[32m+[m[32m// IMPORTANT: Lets call the Blocks part another layer in the fs code structure.[m
[32m+[m
[32m+[m[32m// NOTE: Zero a block on the disk.[m
 static void[m
 bzero(int dev, int bno)[m
 {[m
   struct buf *bp;[m
 [m
[31m-  bp = bread(dev, bno);[m
[31m-  memset(bp->data, 0, BSIZE);[m
[31m-  log_write(bp);[m
[32m+[m[32m  bp = bread(dev, bno);         // read the block into a buffer.[m
[32m+[m[32m  memset(bp->data, 0, BSIZE);   // fill it with zeroes.[m
[32m+[m[32m  log_write(bp);                // write it to disk.[m
   brelse(bp);[m
 }[m
 [m
[31m-// Blocks.[m
[32m+[m[32m// On-disk data Blocks.[m
 [m
[31m-// Allocate a zeroed disk block.[m
[32m+[m[32m// NOTE: Allocate a zeroed disk data block.[m
 static uint[m
 balloc(uint dev)[m
 {[m
[36m@@ -60,15 +63,16 @@[m [mballoc(uint dev)[m
   struct buf *bp;[m
 [m
   bp = 0;[m
[31m-  for(b = 0; b < sb.size; b += BPB){[m
[31m-    bp = bread(dev, BBLOCK(b, sb));[m
[32m+[m[32m  for(b = 0; b < sb.size; b += BPB){        // iterate over ALL FS blocks...[m
[32m+[m[32m    bp = bread(dev, BBLOCK(b, sb));         // fetch the bitmap for the current block number b in bp.[m
[32m+[m[32m    // Keep in mind that there are BPB (512 * 8) number of blocks corresponding to one bitmap block.[m
     for(bi = 0; bi < BPB && b + bi < sb.size; bi++){[m
       m = 1 << (bi % 8);[m
[31m-      if((bp->data[bi/8] & m) == 0){  // Is block free?[m
[31m-        bp->data[bi/8] |= m;  // Mark block in use.[m
[31m-        log_write(bp);[m
[31m-        brelse(bp);[m
[31m-        bzero(dev, b + bi);[m
[32m+[m[32m      if((bp->data[bi/8] & m) == 0){        // Check in the bitmap, is the block free?[m
[32m+[m[32m        bp->data[bi/8] |= m;                // Set the corresponding bit in the bitmap to mark the block in use.[m
[32m+[m[32m        log_write(bp);                      // update the bitmap on the disk[m
[32m+[m[32m        brelse(bp);                         // release that buf.[m
[32m+[m[32m        bzero(dev, b + bi);                 // This will zero the ondisk block[m
         return b + bi;[m
       }[m
     }[m
[36m@@ -84,15 +88,16 @@[m [mbfree(int dev, uint b)[m
   struct buf *bp;[m
   int bi, m;[m
 [m
[31m-  bp = bread(dev, BBLOCK(b, sb));[m
[32m+[m[32m  bp = bread(dev, BBLOCK(b, sb));   // get the corresponding bit block[m
   bi = b % BPB;[m
   m = 1 << (bi % 8);[m
   if((bp->data[bi/8] & m) == 0)[m
     panic("freeing free block");[m
   bp->data[bi/8] &= ~m;[m
[31m-  log_write(bp);[m
[32m+[m[32m  log_write(bp);                    // write the bit block. (note that we do not clear the data block.... we do it at the time of allocation)[m
   brelse(bp);[m
 }[m
[32m+[m[32m//------------------------------------------------------------------------------------------------------------------------------------------------------[m
 [m
 // Inodes.[m
 //[m
[36m@@ -166,7 +171,7 @@[m [mbfree(int dev, uint b)[m
 struct {[m
   struct spinlock lock;[m
   struct inode inode[NINODE];[m
[31m-} icache;[m
[32m+[m[32m} icache;       // cache for holding inodes in-memory[m
 [m
 void[m
 iinit(int dev)[m
[36m@@ -178,7 +183,7 @@[m [miinit(int dev)[m
     initsleeplock(&icache.inode[i].lock, "inode");[m
   }[m
 [m
[31m-  readsb(dev, &sb);[m
[32m+[m[32m  readsb(dev, &sb);       // initialize the global struct superblock[m
   cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\[m
  inodestart %d bmap start %d\n", sb.size, sb.nblocks,[m
           sb.ninodes, sb.nlog, sb.logstart, sb.inodestart,[m
[36m@@ -213,9 +218,9 @@[m [mialloc(uint dev, short type)[m
   panic("ialloc: no inodes");[m
 }[m
 [m
[31m-// Copy a modified in-memory inode to disk.[m
[31m-// Must be called after every change to an ip->xxx field[m
[31m-// that lives on disk, since i-node cache is write-through.[m
[32m+[m[32m// NOTE: Copy a modified in-memory inode to disk.[m
[32m+[m[32m// Must be called after every change to an ip->xxx field that lives on disk (in dinode).[m
[32m+[m[32m// Since i-node cache is WRITE-THROUGH, this function writes the updated inode to disk.[m
 // Caller must hold ip->lock.[m
 void[m
 iupdate(struct inode *ip)[m
[36m@@ -223,21 +228,24 @@[m [miupdate(struct inode *ip)[m
   struct buf *bp;[m
   struct dinode *dip;[m
 [m
[31m-  bp = bread(ip->dev, IBLOCK(ip->inum, sb));[m
[32m+[m[32m  bp = bread(ip->dev, IBLOCK(ip->inum, sb));          // fetch the block containing the inode from the disk[m
   dip = (struct dinode*)bp->data + ip->inum%IPB;[m
[32m+[m[32m  // make changes to the inode in the buf[m
   dip->type = ip->type;[m
   dip->major = ip->major;[m
   dip->minor = ip->minor;[m
   dip->nlink = ip->nlink;[m
   dip->size = ip->size;[m
   memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));[m
[31m-  log_write(bp);[m
[32m+[m[32m  log_write(bp);    // WRITE-THROUGH the disk block[m
   brelse(bp);[m
 }[m
 [m
[31m-// Find the inode with number inum on device dev[m
[31m-// and return the in-memory copy. Does not lock[m
[31m-// the inode and does not read it from disk.[m
[32m+[m[32m// NOTE: Look through the icache for the given device and inode number.[m
[32m+[m[32m// If not found allocate an inode from the icache but MARK IT INVALID.[m
[32m+[m[32m// THIS DOES NOT READ INODE FROM THE DISK.[m
[32m+[m[32m// In either case, return without holding the inode lock.[m
[32m+[m[32m// (Almost like bget in bio.c)[m
 static struct inode*[m
 iget(uint dev, uint inum)[m
 {[m
[36m@@ -265,7 +273,7 @@[m [miget(uint dev, uint inum)[m
   ip->dev = dev;[m
   ip->inum = inum;[m
   ip->ref = 1;[m
[31m-  ip->valid = 0;[m
[32m+[m[32m  ip->valid = 0;        // set the inode as invalid[m
   release(&icache.lock);[m
 [m
   return ip;[m
[36m@@ -282,8 +290,9 @@[m [midup(struct inode *ip)[m
   return ip;[m
 }[m
 [m
[31m-// Lock the given inode.[m
[31m-// Reads the inode from disk if necessary.[m
[32m+[m[32m// NOTE: Lock the given inode.[m
[32m+[m[32m// Getting a lock on an inode isnt the usual acquiresleep()... you need to call ilock.[m
[32m+[m[32m// Also, reads the inode from disk if necessary.[m
 void[m
 ilock(struct inode *ip)[m
 {[m
[36m@@ -311,7 +320,8 @@[m [milock(struct inode *ip)[m
   }[m
 }[m
 [m
[31m-// Unlock the given inode.[m
[32m+[m[32m// Counter to ilock, unlocks the given inode.[m
[32m+[m[32m// Simple releasesleep()[m
 void[m
 iunlock(struct inode *ip)[m
 {[m
[36m@@ -321,13 +331,10 @@[m [miunlock(struct inode *ip)[m
   releasesleep(&ip->lock);[m
 }[m
 [m
[31m-// Drop a reference to an in-memory inode.[m
[31m-// If that was the last reference, the inode cache entry can[m
[31m-// be recycled.[m
[31m-// If that was the last reference and the inode has no links[m
[31m-// to it, free the inode (and its content) on disk.[m
[31m-// All calls to iput() must be inside a transaction in[m
[31m-// case it has to free the inode.[m
[32m+[m[32m// NOTE: Drop a reference to an in-memory inode.[m
[32m+[m[32m// If that was the last reference, the inode cache entry can be recycled.[m
[32m+[m[32m// If that was the last reference and the inode has no links to it, free the inode (and its content) on disk.[m
[32m+[m[32m// All calls to iput() must be inside a transaction (between begin_op() and end_op()) in case it has to free the inode.[m
 void[m
 iput(struct inode *ip)[m
 {[m
[1mdiff --git a/fs.h b/fs.h[m
[1mindex 3214f1d..e1b5c0a 100644[m
[1m--- a/fs.h[m
[1m+++ b/fs.h[m
[36m@@ -27,12 +27,12 @@[m [mstruct superblock {[m
 [m
 // On-disk inode structure[m
 struct dinode {[m
[31m-  short type;           // File type[m
[31m-  short major;          // Major device number (T_DEV only)[m
[31m-  short minor;          // Minor device number (T_DEV only)[m
[31m-  short nlink;          // Number of links to inode in file system[m
[31m-  uint size;            // Size of file (bytes)[m
[31m-  uint addrs[NDIRECT+1];   // Data block addresses[m
[32m+[m[32m  short type;               // File type[m
[32m+[m[32m  short major;              // Major device number (T_DEV only)[m
[32m+[m[32m  short minor;              // Minor device number (T_DEV only)[m
[32m+[m[32m  short nlink;              // Number of links to inode in file system[m[41m [m
[32m+[m[32m  uint size;                // Size of file (bytes)[m
[32m+[m[32m  uint addrs[NDIRECT+1];    // Data block addresses[m
 };[m
 [m
 // Inodes per block.[m
[1mdiff --git a/ide.c b/ide.c[m
[1mindex bdd3b44..ef20bfc 100644[m
[1m--- a/ide.c[m
[1m+++ b/ide.c[m
[36m@@ -56,13 +56,13 @@[m
 //    T0NF = Track 0 Not Found[m
 //    AMNF = Address Mark Not Found[m
 [m
[31m-static struct spinlock idelock;[m
[32m+[m[32mstatic struct spinlock idelock;   // so this lock protects the idequeue[m
 static struct buf *idequeue;[m
 [m
[31m-static int havedisk1;[m
[32m+[m[32mstatic int havedisk1;   //this variable is set to 1 if theres a second hard disk... in ideinit()[m
 static void idestart(struct buf*);[m
 [m
[31m-// NOTE: Wait for IDE disk to become ready.[m
[32m+[m[32m// NOTE: Wait for IDE disk to become ready and THEN return.[m
 static int[m
 idewait(int checkerr)[m
 {[m
[36m@@ -77,13 +77,13 @@[m [midewait(int checkerr)[m
   return 0;[m
 }[m
 [m
[31m-// called from main, in the beginning[m
[32m+[m[32m// called only once from main, in the beginning[m
 void[m
 ideinit(void)[m
 {[m
   int i;[m
 [m
[31m-  initlock(&idelock, "ide");[m
[32m+[m[32m  initlock(&idelock, "ide");    // initialize the lock that protect the idequeue[m
   ioapicenable(IRQ_IDE, ncpu - 1);[m
   idewait(0);[m
 [m
[36m@@ -105,7 +105,7 @@[m [mideinit(void)[m
 static void[m
 idestart(struct buf *b)[m
 {[m
[31m-  // NOTE: Starts a queued request.[m
[32m+[m[32m  // NOTE: Starts operation of the buf passed to it[m
 [m
   if(b == 0)[m
     panic("idestart");[m
[36m@@ -125,6 +125,8 @@[m [midestart(struct buf *b)[m
   outb(0x1f4, (sector >> 8) & 0xff);[m
   outb(0x1f5, (sector >> 16) & 0xff);[m
   outb(0x1f6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));[m
[32m+[m
[32m+[m[32m  // NOTE: iderw has checked the flags... if dirty then its a write operation if not its a read operation[m
   if(b->flags & B_DIRTY){[m
     outb(0x1f7, write_cmd);[m
     outsl(0x1f0, b->data, BSIZE/4);[m
[36m@@ -143,22 +145,25 @@[m [mideintr(void)[m
   acquire(&idelock);[m
 [m
   if((b = idequeue) == 0){[m
[32m+[m[32m    // QUESTION: Why is this here? If the intr was for some operation then the queue wouldnt be empty?[m
[32m+[m[32m    // Can the disk controller unnecessarily cause hardware interrupts ?[m
     release(&idelock);[m
     return;[m
   }[m
[31m-  idequeue = b->qnext;[m
 [m
[31m-  // Read data if needed.[m
[32m+[m[32m  idequeue = b->qnext;    // advance in the queue[m
[32m+[m
[32m+[m[32m  // Read data from the disk controller memory into the RAM(buf) if it was a read operation[m
   if(!(b->flags & B_DIRTY) && idewait(1) >= 0)[m
     insl(0x1f0, b->data, BSIZE/4);[m
 [m
   // Wake process waiting for this buf.[m
[31m-  b->flags |= B_VALID;[m
[31m-  b->flags &= ~B_DIRTY;[m
[32m+[m[32m  b->flags |= B_VALID;    //set valid bit... regardless of whether it was read or write, the valid bit = 1[m
[32m+[m[32m  b->flags &= ~B_DIRTY;   //reset dirty bit... regardless of whether it was read or write, the dirty bit = 1[m
   wakeup(b);  // QUESTION: How does this process sleeping on a blocking call work? Whats chan?[m
               // And how exactly is the requesting process identified ?[m
 [m
[31m-  // Start disk on next buf in queue.[m
[32m+[m[32m  // Start disk on next buf in queue if the queue is not empty.[m
   if(idequeue != 0)[m
     idestart(idequeue);[m
 [m
[36m@@ -172,15 +177,18 @@[m [mideintr(void)[m
 void[m
 iderw(struct buf *b)[m
 {[m
[31m-  // NOTE: enqueues the request and starts the request if thats the only request in the queue.[m
[32m+[m[32m  // NOTE:The device driver is the bridge between the device controller and the OS,[m
[32m+[m[32m  // so it has to understand how the OS internally uses the data (struct buf)[m
[32m+[m[32m  // and it also has to understand how the disk controller works (whatever protocol)[m
[32m+[m[32m  // This function enqueues the request and starts the request if thats the only request in the queue.[m
 [m
   struct buf **pp;[m
 [m
   if(!holdingsleep(&b->lock))[m
     panic("iderw: buf not locked");[m
[31m-  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)[m
[32m+[m[32m  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)   // if its not dirty but valid means its an error[m
     panic("iderw: nothing to do");[m
[31m-  if(b->dev != 0 && !havedisk1)[m
[32m+[m[32m  if(b->dev != 0 && !havedisk1)                   // if device no = 1 and we dont have a second disk then error[m
     panic("iderw: ide disk 1 not present");[m
 [m
   acquire(&idelock);  //DOC:acquire-lock[m
[36m@@ -192,7 +200,8 @@[m [miderw(struct buf *b)[m
   *pp = b;[m
 [m
   // Start disk if necessary.[m
[31m-  // (if its not the only request in the queue, then we already know that the one that is at the front has already been started and so we cant start...)[m
[32m+[m[32m  // NOTE:[m
[32m+[m[32m  // if b is the only buf in the queue then it means that there was no buf in the queue before, and that the disk is free and thus we need to idestart() the operation[m
   // if a request is already being serviced then the idestart on our request will be run by the ideintr handler... It calls idestart after wakeup[m
   if(idequeue == b)[m
     idestart(b);[m
[1mdiff --git a/log.c b/log.c[m
[1mindex a64c0f6..5e53cb9 100644[m
[1m--- a/log.c[m
[1m+++ b/log.c[m
[36m@@ -31,25 +31,30 @@[m
 [m
 // Contents of the header block, used for both the on-disk header block[m
 // and to keep track in memory of logged block# before commit.[m
[31m-struct logheader {[m
[31m-  int n;[m
[31m-  int block[LOGSIZE];[m
[31m-};[m
[32m+[m[32mstruct logheader {      // NOTE: We get this structure's address from the super block... (and we make the struct log below)[m
[32m+[m[32m  int n;                // total number of valid block numbers in the following array[m
[32m+[m[32m  int block[LOGSIZE];   // contains the block number where the data block goes...[m
[32m+[m[32m};                      // this struct is an ON-DISK data structure.[m
[32m+[m[32m                        // The subsequent blocks on the disk after the header contain the data blocks corresponding to the block numbers in the array.[m
[32m+[m[32m                        // See supplementary_figures/xv6_fslog.png[m
 [m
[31m-struct log {[m
[31m-  struct spinlock lock;[m
[31m-  int start;[m
[31m-  int size;[m
[31m-  int outstanding; // how many FS sys calls are executing.[m
[31m-  int committing;  // in commit(), please wait.[m
[31m-  int dev;[m
[31m-  struct logheader lh;[m
[32m+[m[32mstruct log {            // NOTE: we get the following data from the super block[m
[32m+[m[32m  struct spinlock lock; // lock protecting this structure.[m[41m  [m
[32m+[m[32m  int start;            // the first log block on the disk... from the super block[m
[32m+[m[32m  int size;             // total number of log data blocks that are in use out of 30[m[41m [m
[32m+[m[32m  int outstanding;      // how many FS sys calls are currently executing.[m
[32m+[m[32m  int committing;       // in commit(), please wait until the commit is over.[m
[32m+[m[32m  int dev;              // the device number to identify the disk containing the file system (ie. fs.img)[m
[32m+[m[32m  struct logheader lh;  // the IN-MEMORY copy of the logheader from the disk[m
 };[m
 struct log log;[m
 [m
 static void recover_from_log(void);[m
 static void commit();[m
 [m
[32m+[m[32m// QUESTION: Why is this not from the main() function, and instead called from forkret()'s first run?[m
[32m+[m[32m// NOTE: Initializes the struct log.[m
[32m+[m[32m// Get the super block, and initialize the in-memory struct log from the super block[m
 void[m
 initlog(int dev)[m
 {[m
[36m@@ -59,26 +64,27 @@[m [minitlog(int dev)[m
   struct superblock sb;[m
   initlock(&log.lock, "log");[m
   readsb(dev, &sb);[m
[31m-  log.start = sb.logstart;[m
[32m+[m[32m  log.start = sb.logstart;[m[41m    [m
   log.size = sb.nlog;[m
   log.dev = dev;[m
   recover_from_log();[m
 }[m
 [m
[31m-// Copy committed blocks from log to their home location[m
[32m+[m[32m// NOTE: Copy committed blocks from log to their home location[m
[32m+[m[32m// All valid log data blocks to their corresponding locations in the block[m
 static void[m
 install_trans(void)[m
 {[m
   int tail;[m
 [m
   for (tail = 0; tail < log.lh.n; tail++) {[m
[31m-    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block[m
[31m-    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst[m
[31m-    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst[m
[31m-    bwrite(dbuf);  // write dst to disk[m
[32m+[m[32m    struct buf *lbuf = bread(log.dev, log.start+tail+1);    // read log block[m
[32m+[m[32m    struct buf *dbuf = bread(log.dev, log.lh.block[tail]);  // read dst[m
[32m+[m[32m    memmove(dbuf->data, lbuf->data, BSIZE);                 // copy block to dst[m
[32m+[m[32m    bwrite(dbuf);                                           // write dst to disk[m
     brelse(lbuf);[m
     brelse(dbuf);[m
[31m-  }[m
[32m+[m[32m  } // do this for every valid log data block[m
 }[m
 [m
 // Read the log header from disk into the in-memory log header[m
[36m@@ -95,44 +101,51 @@[m [mread_head(void)[m
   brelse(buf);[m
 }[m
 [m
[31m-// Write in-memory log header to disk.[m
[31m-// This is the true point at which the[m
[31m-// current transaction commits.[m
[32m+[m[32m// NOTE: Write in-memory log header to disk.[m
[32m+[m[32m// This is the true point at which the current transaction commits.[m
 static void[m
 write_head(void)[m
 {[m
[31m-  struct buf *buf = bread(log.dev, log.start);[m
[32m+[m[32m  struct buf *buf = bread(log.dev, log.start);              // read the block containing the on-disk logheader into a buf[m
   struct logheader *hb = (struct logheader *) (buf->data);[m
   int i;[m
[32m+[m[32m  // copy the in-memory logheader to the buf[m
   hb->n = log.lh.n;[m
   for (i = 0; i < log.lh.n; i++) {[m
     hb->block[i] = log.lh.block[i];[m
   }[m
[31m-  bwrite(buf);[m
[31m-  brelse(buf);[m
[32m+[m[32m  bwrite(buf);    // write the buf to the disk.[m
[32m+[m[32m  brelse(buf);    // we are done with the buf now.[m
 }[m
 [m
[32m+[m[32m// NOTE: Only called from initlog.[m
 static void[m
 recover_from_log(void)[m
 {[m
   read_head();[m
[31m-  install_trans(); // if committed, copy from log to disk[m
[31m-  log.lh.n = 0;[m
[31m-  write_head(); // clear the log[m
[32m+[m[32m  install_trans();  // if committed, copy from log to disk... Complete the incomplete transaction.[m
[32m+[m[32m  log.lh.n = 0;     // clear the on-disk logheader.[m
[32m+[m[32m  write_head();     // clear the log[m
 }[m
 [m
[31m-// called at the start of each FS system call.[m
[32m+[m[32m// NOTE: Called at the start of each FS system call.[m
 void[m
 begin_op(void)[m
 {[m
   acquire(&log.lock);[m
   while(1){[m
     if(log.committing){[m
[31m-      sleep(&log, &log.lock);[m
[32m+[m[32m      // if we are currently commiting then we need to wait until the transaction is fully completed (committed) so go to sleep[m
[32m+[m[32m      sleep(&log, &log.lock);[m[41m       [m
     } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){[m
[31m-      // this op might exhaust log space; wait for commit.[m
[32m+[m[32m      // There is a possibility that this system call (begin_op is called at the beginning of every fs system call)[m
[32m+[m[32m      // will cause the struct logheader to get full. Thus wait for previous FS syscalls to get over and then committed.[m
[32m+[m[32m      // How to decide that ? We are assuming that an FS call will only write upto 10 blocks.[m
[32m+[m[32m      // So go to sleep.[m
       sleep(&log, &log.lock);[m
     } else {[m
[32m+[m[32m      // if neither of the above two problems then ok[m
[32m+[m[32m      // increment outstanding fields to show that one more fs system call is active.[m
       log.outstanding += 1;[m
       release(&log.lock);[m
       break;[m
[36m@@ -140,8 +153,8 @@[m [mbegin_op(void)[m
   }[m
 }[m
 [m
[31m-// called at the end of each FS system call.[m
[31m-// commits if this was the last outstanding operation.[m
[32m+[m[32m// NOTE: Called at the end of each FS system call.[m
[32m+[m[32m// Commits if this was the last outstanding FS system call.[m
 void[m
 end_op(void)[m
 {[m
[36m@@ -150,14 +163,17 @@[m [mend_op(void)[m
   acquire(&log.lock);[m
   log.outstanding -= 1;[m
   if(log.committing)[m
[32m+[m[32m    // if we are committing that means that the last active FS system call is over. How is this getting called again? -> PANIC[m
     panic("log.committing");[m
[31m-  if(log.outstanding == 0){[m
[32m+[m[32m  if(log.outstanding == 0){[m[41m   [m
[32m+[m[32m    // if this was the last remaining FS system call, then we want to commit.[m
[32m+[m[32m    // IMPORTANT: The last FS system call (or last operation) getting finished marks the END of a TRANSACTION.[m
[32m+[m[32m    // Similarly the outstanding value changing from 0 to 1 marks the START of a TRANSACTION.[m
     do_commit = 1;[m
     log.committing = 1;[m
   } else {[m
[31m-    // begin_op() may be waiting for log space,[m
[31m-    // and decrementing log.outstanding has decreased[m
[31m-    // the amount of reserved space.[m
[32m+[m[32m    // begin_op() may be waiting for log space, and decrementing log.outstanding has decreased the amount of reserved space.[m
[32m+[m[32m    // So try calling wakeup if some begin_op() was waiting to allow it.[m
     wakeup(&log);[m
   }[m
   release(&log.lock);[m
[36m@@ -165,70 +181,79 @@[m [mend_op(void)[m
   if(do_commit){[m
     // call commit w/o holding locks, since not allowed[m
     // to sleep with locks.[m
[31m-    commit();[m
[32m+[m[32m    commit();   // take data from the log area of the disk and write into the actual data blocks.[m
     acquire(&log.lock);[m
[32m+[m[32m    // once the commit is over, acquire spinlock, reset committing and wakeup() any process that might be waiting in begin_op for commit() to get over.[m
     log.committing = 0;[m
     wakeup(&log);[m
     release(&log.lock);[m
   }[m
 }[m
 [m
[31m-// Copy modified blocks from cache to log.[m
[32m+[m[32m// NOTE: Copy modified blocks from cache to log. (In log_write we only noted the block number in the in memory struct log)[m
[32m+[m[32m// Only called from commit().[m
 static void[m
 write_log(void)[m
 {[m
   int tail;[m
 [m
   for (tail = 0; tail < log.lh.n; tail++) {[m
[31m-    struct buf *to = bread(log.dev, log.start+tail+1); // log block[m
[31m-    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block[m
[31m-    memmove(to->data, from->data, BSIZE);[m
[31m-    bwrite(to);  // write the log[m
[31m-    brelse(from);[m
[31m-    brelse(to);[m
[31m-  }[m
[32m+[m[32m    struct buf *to = bread(log.dev, log.start+tail+1);      // get a log block (basically get a buffer and the meta data of the data block in the log section)[m
[32m+[m[32m    struct buf *from = bread(log.dev, log.lh.block[tail]);  // cache block (get a pointer to the buf that we want to write to disk)[m
[32m+[m[32m    memmove(to->data, from->data, BSIZE);                   // copy data to the log block.[m
[32m+[m[32m    bwrite(to);     // write the log to disk[m
[32m+[m[32m    brelse(from);   // we are done with the buf, we wrote it to the disk (in the log block not the actual file system yet.)[m
[32m+[m[32m    brelse(to);     // we are done with the log block as well[m
[32m+[m[32m  } // do this for every valid block number in the in memory logheader.[m
 }[m
 [m
[32m+[m[32m// NOTE: Only called from end_op().[m
 static void[m
 commit()[m
 {[m
[31m-  if (log.lh.n > 0) {[m
[31m-    write_log();     // Write modified blocks from cache to log[m
[31m-    write_head();    // Write header to disk -- the real commit[m
[31m-    install_trans(); // Now install writes to home locations[m
[31m-    log.lh.n = 0;[m
[31m-    write_head();    // Erase the transaction from the log[m
[32m+[m[32m  if (log.lh.n > 0) { // if the log contains something[m
[32m+[m[32m    write_log();      // Write modified blocks from buffer cache to log blocks[m
[32m+[m[32m    write_head();     // Write logheader to disk -- the true commit[m
[32m+[m[32m    install_trans();  // Now install writes to home locations (copy from log data blocks to actual blocks in the FS)[m
[32m+[m[32m    log.lh.n = 0;     // reset the in memory logheader because all pending writes are complete now. The TRANSACTION is complete.[m
[32m+[m[32m    write_head();     // Erase the transaction from the on-disk logheader.[m
   }[m
 }[m
 [m
[31m-// Caller has modified b->data and is done with the buffer.[m
[31m-// Record the block number and pin in the cache with B_DIRTY.[m
[31m-// commit()/write_log() will do the disk write.[m
[32m+[m[32m// IMPORTANT:[m[41m [m
[32m+[m[32m// Usage: Caller has modified b->data and is done with the buffer.[m
[32m+[m[32m// Now record the block number and pin in the cache with B_DIRTY.[m
[32m+[m[32m// Now call log_write to ONLY NOTE DOWN THE BLOCK NUMBER IN THE logheader.[m
[32m+[m[32m// commit()/write_log() will do the actual disk write.[m
 //[m
[32m+[m[32m// IMPORTANT:[m
 // log_write() replaces bwrite(); a typical use is:[m
[31m-//   bp = bread(...)[m
[31m-//   modify bp->data[][m
[31m-//   log_write(bp)[m
[31m-//   brelse(bp)[m
[32m+[m[32m//    bp = bread(...)[m
[32m+[m[32m//    modify bp->data[][m
[32m+[m[32m//    log_write(bp)[m
[32m+[m[32m//    brelse(bp)[m
 void[m
 log_write(struct buf *b)[m
 {[m
   int i;[m
 [m
   if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)[m
[31m-    panic("too big a transaction");[m
[32m+[m[32m    panic("too big a transaction");       // if you do too many operations in a single transaction -> panic[m
   if (log.outstanding < 1)[m
[31m-    panic("log_write outside of trans");[m
[32m+[m[32m    panic("log_write outside of trans");[m[41m  [m
 [m
   acquire(&log.lock);[m
[32m+[m[32m  // iterate over all the valid block numbers in logheader.[m
[32m+[m[32m  // if the block number already exists use that.[m
[32m+[m[32m  // if not select the next unused slot in logheader's array of block numbers.[m
   for (i = 0; i < log.lh.n; i++) {[m
     if (log.lh.block[i] == b->blockno)   // log absorbtion[m
       break;[m
   }[m
   log.lh.block[i] = b->blockno;[m
   if (i == log.lh.n)[m
[31m-    log.lh.n++;[m
[31m-  b->flags |= B_DIRTY; // prevent eviction[m
[32m+[m[32m    log.lh.n++;         // update the value of n in logheader if a new slot was used.[m
[32m+[m[32m  b->flags |= B_DIRTY;  // prevent eviction from the buffer cache.[m
   release(&log.lock);[m
 }[m
 [m
[1mdiff --git a/mmu.h b/mmu.h[m
[1mindex 71b7425..7791f3b 100644[m
[1m--- a/mmu.h[m
[1m+++ b/mmu.h[m
[36m@@ -167,8 +167,8 @@[m [mstruct gatedesc {[m
 //        this interrupt/trap gate explicitly using an int instruction.[m
 [m
 // NOTE: COOOOOL this is how you write a multiline macro in C...[m
[31m-[m
[31m-#define SETGATE(gate, istrap, sel, off, d)                \[m
[32m+[m[32m// Btw, istrap means that the IF flag isnt cleared when this interrupt occurs[m
[32m+[m[32m#define SETGATE(gate, istrap, sel, off, d)                \[m[41m [m
 {                                                         \[m
   (gate).off_15_0 = (uint)(off) & 0xffff;                 \[m
   (gate).cs = (sel);                                      \[m
[1mdiff --git a/proc.c b/proc.c[m
[1mindex 0d5e7f8..f2bc49d 100644[m
[1m--- a/proc.c[m
[1m+++ b/proc.c[m
[36m@@ -401,9 +401,9 @@[m [msched(void)[m
   int intena;[m
   struct proc *p = myproc();[m
 [m
[31m-  if(!holding(&ptable.lock))[m
[32m+[m[32m  if(!holding(&ptable.lock))      // checks if you are holding the ptable lock... YOU MUST![m
     panic("sched ptable.lock");[m
[31m-  if(mycpu()->ncli != 1)[m
[32m+[m[32m  if(mycpu()->ncli != 1)          // and you must be only holding the ptable lock thats IT! (general OS rule, dont block while holding a lock...)[m
     panic("sched locks");[m
   if(p->state == RUNNING)[m
     panic("sched running");[m
[36m@@ -455,6 +455,9 @@[m [mforkret(void)[m
 void[m
 sleep(void *chan, struct spinlock *lk)[m
 {[m
[32m+[m[32m  // IMPORTANT: We were holding lk lock and now we want to save chan to mycpu()->chan and go to sleep[m
[32m+[m[32m  // (We identify what a process is waiting for when SLEEPING using the value at chan)[m
[32m+[m[41m  [m
   struct proc *p = myproc();[m
   [m
   if(p == 0)[m
[36m@@ -463,7 +466,7 @@[m [msleep(void *chan, struct spinlock *lk)[m
   if(lk == 0)[m
     panic("sleep without lk");[m
 [m
[31m-  // Must acquire ptable.lock in order to[m
[32m+[m[32m  // IMPORTANT: Must acquire ptable.lock in order to[m
   // change p->state and then call sched.[m
   // Once we hold ptable.lock, we can be[m
   // guaranteed that we won't miss any wakeup[m
[36m@@ -473,8 +476,14 @@[m [msleep(void *chan, struct spinlock *lk)[m
     acquire(&ptable.lock);  //DOC: sleeplock1[m
     release(lk);[m
   }[m
[32m+[m[32m  // IMPORTANT: I know we said that do not go to sleep while holding a lock... The rule seems to get violated here when we call sched...[m
[32m+[m[32m  // But it isnt...[m[41m [m
[32m+[m[32m  // Think of it this way, sched() causes the scheduler to run on its own stack...[m
[32m+[m[32m  // If you look at the code of scheduler() then you will realise that in order for it to run it must hold the ptable lock...[m
[32m+[m[32m  // So we are just acquiring that lock and asking the scheduler to continue wherever it left off.[m
[32m+[m
   // Go to sleep.[m
[31m-  p->chan = chan;[m
[32m+[m[32m  p->chan = chan;     //let chan point to something which tells us what this process is waiting for.[m
   p->state = SLEEPING;[m
 [m
   sched();[m
[36m@@ -482,7 +491,7 @@[m [msleep(void *chan, struct spinlock *lk)[m
   // Tidy up.[m
   p->chan = 0;[m
 [m
[31m-  // Reacquire original lock.[m
[32m+[m[32m  // NOTE: Now that we have been woken up, Reacquire original lock and also let go of ptable lock.[m
   if(lk != &ptable.lock){  //DOC: sleeplock2[m
     release(&ptable.lock);[m
     acquire(lk);[m
[36m@@ -490,7 +499,7 @@[m [msleep(void *chan, struct spinlock *lk)[m
 }[m
 [m
 //PAGEBREAK![m
[31m-// Wake up all processes sleeping on chan.[m
[32m+[m[32m// NOTE: Wake up ALL processes sleeping on chan.[m
 // The ptable lock must be held.[m
 static void[m
 wakeup1(void *chan)[m
[36m@@ -502,7 +511,7 @@[m [mwakeup1(void *chan)[m
       p->state = RUNNABLE;[m
 }[m
 [m
[31m-// Wake up all processes sleeping on chan.[m
[32m+[m[32m// Wake up ALL processes sleeping on chan.[m
 void[m
 wakeup(void *chan)[m
 {[m
[1mdiff --git a/proc.h b/proc.h[m
[1mindex b61598c..667d52f 100644[m
[1m--- a/proc.h[m
[1m+++ b/proc.h[m
[36m@@ -9,7 +9,12 @@[m [mstruct cpu {[m
   struct taskstate ts;         // Used by x86 to find stack for interrupt (the per process kernel stack)[m
   struct segdesc gdt[NSEGS];   // x86 global descriptor table[m
   volatile uint started;       // Has the CPU started?[m
[31m-  int ncli;                    // Depth of pushcli nesting.[m
[32m+[m[32m  int ncli;                    // NOTE: Depth of pushcli nesting. (how many pushcli()s have been called ?)[m
[32m+[m[32m                               // But, wherever pushcli() has been called, popcli() also has been called immediately EXCEPT where the lock acquire and release are...[m
[32m+[m[32m                               // In acquire(), pushcli() is called and acquire() returns...[m
[32m+[m[32m                               // In release(), popcli() has been called....[m
[32m+[m[32m                               // So basically whenever the ncli is really consulted anywhere in the kernel code, it basically shows the number of locks held...[m[41m [m
[32m+[m[41m                               [m
   int intena;                  // Were interrupts enabled before pushcli?[m
   struct proc *proc;           // The process running on this cpu or null[m
 };[m
[36m@@ -56,7 +61,7 @@[m [mstruct proc {[m
                                // It doesnt always hold the current context.[m
                                // It only holds the context when the process is switched out.[m
 [m
[31m-  void *chan;                  // If non-zero, sleeping on chan[m
[32m+[m[32m  void *chan;                  // If non-zero, sleeping on chan... (when the status is WAITING, this field is non zero and points to something that tells us what it is waiting for)[m
   int killed;                  // If non-zero, have been killed[m
   struct file *ofile[NOFILE];  // Open files[m
   struct inode *cwd;           // Current directory[m
[1mdiff --git a/sleeplock.c b/sleeplock.c[m
[1mindex e0750ea..d16969e 100644[m
[1m--- a/sleeplock.c[m
[1m+++ b/sleeplock.c[m
[36m@@ -13,32 +13,35 @@[m
 void[m
 initsleeplock(struct sleeplock *lk, char *name)[m
 {[m
[32m+[m[32m  // All sleeplock must be initialized before being used[m
   initlock(&lk->lk, "sleep lock");[m
   lk->name = name;[m
   lk->locked = 0;[m
   lk->pid = 0;[m
 }[m
 [m
[32m+[m[32m// IMPORTANT: One thing to notice is that a sleep lock when acquired doesnt cause interrupts to be disabled...[m
[32m+[m[32m// Its pretty obvious why; if the thread goes to sleep while disabling the interrupts then thats a deadlock....[m
 void[m
 acquiresleep(struct sleeplock *lk)[m
 {[m
[31m-  acquire(&lk->lk);[m
[32m+[m[32m  acquire(&lk->lk);   // acquire the spinlock protecting the sleeplock[m
   while (lk->locked) {[m
[31m-    sleep(lk, &lk->lk);[m
[32m+[m[32m    sleep(lk, &lk->lk);   // if the lock is already held by someone, block[m
   }[m
   lk->locked = 1;[m
   lk->pid = myproc()->pid;[m
[31m-  release(&lk->lk);[m
[32m+[m[32m  release(&lk->lk);   // release the spinlock protecting the sleeplock[m
 }[m
 [m
 void[m
 releasesleep(struct sleeplock *lk)[m
 {[m
[31m-  acquire(&lk->lk);[m
[32m+[m[32m  acquire(&lk->lk);   // acquire the spinlock protecting the spinlock[m
   lk->locked = 0;[m
   lk->pid = 0;[m
   wakeup(lk);[m
[31m-  release(&lk->lk);[m
[32m+[m[32m  release(&lk->lk);   // release the spinlock protecting the sleeplock[m
 }[m
 [m
 int[m
[1mdiff --git a/spinlock.c b/spinlock.c[m
[1mindex 4020186..46134af 100644[m
[1m--- a/spinlock.c[m
[1m+++ b/spinlock.c[m
[36m@@ -12,6 +12,7 @@[m
 void[m
 initlock(struct spinlock *lk, char *name)[m
 {[m
[32m+[m[32m  // NOTE: Each lock must first be initialized...[m
   lk->name = name;[m
   lk->locked = 0;[m
   lk->cpu = 0;[m
[36m@@ -26,16 +27,18 @@[m [macquire(struct spinlock *lk)[m
 {[m
   pushcli(); // disable interrupts to avoid deadlock.[m
   if(holding(lk))[m
[31m-    panic("acquire");[m
[32m+[m[32m    panic("acquire");[m[41m   [m
[32m+[m[32m  //if the current cpu is already holding the lock and its trying to acquire the lock again then we have a problem[m
[32m+[m[32m  // QUESTION: Why the current cpu ? I dont understand... Any other process that was running might have acquired the lock...? Whats the problem ?[m
 [m
[31m-  // The xchg is atomic.[m
[32m+[m[32m  // The xchg is atomic. Actually acquire the lock[m
   while(xchg(&lk->locked, 1) != 0)[m
     ;[m
 [m
   // Tell the C compiler and the processor to not move loads or stores[m
   // past this point, to ensure that the critical section's memory[m
   // references happen after the lock is acquired.[m
[31m-  __sync_synchronize();[m
[32m+[m[32m  __sync_synchronize();   // A "compiler directive"[m
 [m
   // Record info about lock acquisition for debugging.[m
   lk->cpu = mycpu();[m
[36m@@ -47,7 +50,7 @@[m [mvoid[m
 release(struct spinlock *lk)[m
 {[m
   if(!holding(lk))[m
[31m-    panic("release");[m
[32m+[m[32m    panic("release");   // if you are not holding and then have called release then its a problem[m
 [m
   lk->pcs[0] = 0;[m
   lk->cpu = 0;[m
[36m@@ -74,7 +77,10 @@[m [mgetcallerpcs(void *v, uint pcs[])[m
   uint *ebp;[m
   int i;[m
 [m
[31m-  ebp = (uint*)v - 2;[m
[32m+[m[32m  // QUESTION: Okay i get what this is doing[m
[32m+[m[32m  // Its traversing the chain of frame pointers to get the function call sequence...[m
[32m+[m[32m  // but i still want to dry run it and see it working...[m
[32m+[m[32m  ebp = (uint*)v - 2;   //[m
   for(i = 0; i < 10; i++){[m
     if(ebp == 0 || ebp < (uint*)KERNBASE || ebp == (uint*)0xffffffff)[m
       break;[m
[36m@@ -91,16 +97,15 @@[m [mholding(struct spinlock *lock)[m
 {[m
   int r;[m
   pushcli();[m
[31m-  r = lock->locked && lock->cpu == mycpu();[m
[32m+[m[32m  r = lock->locked && lock->cpu == mycpu();   //return true if the current cpu is already holding a lock[m
   popcli();[m
   return r;[m
 }[m
 [m
 [m
 // Pushcli/popcli are like cli/sti except that they are matched:[m
[31m-// it takes two popcli to undo two pushcli.  Also, if interrupts[m
[32m+[m[32m// eg: it takes two popcli to undo two pushcli.  Also, if interrupts[m
 // are off, then pushcli, popcli leaves them off.[m
[31m-[m
 void[m
 pushcli(void)[m
 {[m
[36m@@ -109,18 +114,19 @@[m [mpushcli(void)[m
   eflags = readeflags();[m
   cli();[m
   if(mycpu()->ncli == 0)[m
[31m-    mycpu()->intena = eflags & FL_IF;[m
[31m-  mycpu()->ncli += 1;[m
[32m+[m[32m    mycpu()->intena = eflags & FL_IF;   // save the flag only if its the first pushcli[m
[32m+[m[32m  mycpu()->ncli += 1;   // ncli field effectively saves how many pushcli()s have happened. (go to its comment in struct cpu)[m
 }[m
 [m
 void[m
 popcli(void)[m
 {[m
[32m+[m[32m  // NOTE: Decrement ncli, if its zero that means all pushcli()s have been undone... restore the interrupt flag depending on intena value.[m
   if(readeflags()&FL_IF)[m
[31m-    panic("popcli - interruptible");[m
[32m+[m[32m    panic("popcli - interruptible");    // if the flag is set then calling popcli is wrong[m
   if(--mycpu()->ncli < 0)[m
     panic("popcli");[m
   if(mycpu()->ncli == 0 && mycpu()->intena)[m
[31m-    sti();[m
[32m+[m[32m    sti();      // if interrupt was enabled before the first cli only then set IF[m
 }[m
 [m
[1mdiff --git a/syscall.c b/syscall.c[m
[1mindex 1ee25ae..cc42f94 100644[m
[1m--- a/syscall.c[m
[1m+++ b/syscall.c[m
[36m@@ -34,7 +34,8 @@[m [mfetchint(uint addr, int *ip)[m
 {[m
   struct proc *curproc = myproc();[m
 [m
[31m-  if(addr >= curproc->sz || addr+4 > curproc->sz)[m
[32m+[m[32m  if(addr >= curproc->sz || addr+4 > curproc->sz)     // before using the addr as a pointer, check if it lies in the users address space.[m
[32m+[m[32m                                                      // the user could have changed to an invalid esp and doing something malicious[m
     return -1;[m
   *ip = *(int*)(addr);[m
   return 0;[m
[36m@@ -78,7 +79,10 @@[m [margptr(int n, char **pp, int size)[m
  [m
   if(argint(n, &i) < 0)[m
     return -1;[m
[31m-  if(size < 0 || (uint)i >= curproc->sz || (uint)i+size > curproc->sz)[m
[32m+[m[32m  if(size < 0 || (uint)i >= curproc->sz || (uint)i+size > curproc->sz)    // check if the pointer is a valid pointer in the user's address space[m
[32m+[m[32m                                                                          // if its not and the kernel dereferences it, it will cause a seg fault (kernel mode seg fualt)[m
[32m+[m[32m                                                                          // and it will go into trap and get handled there... except in our code it checks whether the execution was in kernel mode[m
[32m+[m[32m                                                                          // if yes, ie the page fault was in kernel then it calls panic... (see default label in trap())[m
     return -1;[m
   *pp = (char*)i;[m
   return 0;[m
[1mdiff --git a/trap.c b/trap.c[m
[1mindex 8e70acb..af2e637 100644[m
[1m--- a/trap.c[m
[1m+++ b/trap.c[m
[36m@@ -95,17 +95,19 @@[m [mtrap(struct trapframe *tf)[m
   //PAGEBREAK: 13[m
   default:[m
     if(myproc() == 0 || (tf->cs&3) == 0){[m
[31m-      // In kernel, it must be our mistake.[m
[32m+[m[32m      // In kernel, it must be our mistake. Executed some int x instruction... for some invalid x or maybe some exception...[m
       cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",[m
               tf->trapno, cpuid(), tf->eip, rcr2());[m
       panic("trap");[m
     }[m
     // In user space, assume process misbehaved.[m
[32m+[m[32m    // NOTE: How can this ever happen? I think if it does int x where x is anything other than T_SYSCALL, then it will cause int 13 which is general protection fault.[m
[32m+[m[32m    // Thus this must be some exception (like zero division...) or unlikely but not impossible, it could be some bad hardware interrupt...[m
     cprintf("pid %d %s: trap %d err %d on cpu %d "[m
             "eip 0x%x addr 0x%x--kill proc\n",[m
             myproc()->pid, myproc()->name, tf->trapno,[m
             tf->err, cpuid(), tf->eip, rcr2());[m
[31m-    myproc()->killed = 1;[m
[32m+[m[32m    myproc()->killed = 1;   // note that this process must be killed and cleaned up[m
   }[m
 [m
   // Force process exit if it has been killed and is in user space.[m
[1mdiff --git a/trapasm.S b/trapasm.S[m
[1mindex fbb3890..a6a5dfa 100644[m
[1m--- a/trapasm.S[m
[1m+++ b/trapasm.S[m
[36m@@ -20,7 +20,7 @@[m [malltraps:[m
   pushl %gs[m
   pushal[m
   [m
[31m-  # Set up data segments.[m
[32m+[m[32m  # Set up data segments. (ss was set to SEG_KDATA through the TSS.ss entry by the hardware but the cs and es havent been set to SEG_KDATA yet...)[m
   movw $(SEG_KDATA<<3), %ax[m
   movw %ax, %ds[m
   movw %ax, %es[m
