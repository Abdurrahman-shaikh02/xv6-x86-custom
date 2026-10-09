struct buf {
  int flags;
  uint dev;     //identify which disk has this data been read from
  uint blockno; 
  struct sleeplock lock;    //every buf has a lock
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  struct buf *qnext; // disk queue (this is used when it has to be queued in the idequeue... it forms a linked list.)
  uchar data[BSIZE];
};

// NOTE: the following flags tell iderw what the operation to perform is...
#define B_VALID 0x2  // buffer has been read from disk
#define B_DIRTY 0x4  // buffer needs to be written to disk

