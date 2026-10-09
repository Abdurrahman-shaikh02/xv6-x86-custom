struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;   // a file of 3 types (an inode is of three type, T_DIR, T_DEV, T_FILE)(and a file is again of these 3 types)
  int ref;                                    // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};


// NOTE: IN-MEMORY copy of an inode
// The on-disk inode struct is different... its called struct dinode.
struct inode {
  uint dev;               // Device number
  uint inum;              // Inode number
  int ref;                // Reference count
  struct sleeplock lock;  // protects everything below here. QUESTION: Why use a sleeplock here? and not a spinlock?
  int valid;              // inode has been read from disk?

  short type;             // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};

// table mapping major device number to
// device functions
struct devsw {                                  // three arguments each
  int (*read)(struct inode*, char*, int);       // which inode? read to where? how much to read?
  int (*write)(struct inode*, char*, int);      // which inode? write from where? how much to write?
};

extern struct devsw devsw[];

#define CONSOLE 1
