#ifndef SPINLOCK_H
#define SPINLOCK_H
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.
};
#endif

struct swap_table {
  struct spinlock lock;
  int nfree;
  char *bitmap;
};

extern struct swap_table stable;
extern void stable_init();
extern int stable_set_blk(int blkno);
extern void stable_free_blk(int blkno);
extern int stable_get_freeblk();
int page_fault_handler(uint addr);

extern pte_t * walkpgdir(pde_t *pgdir, const void *va, int alloc);
