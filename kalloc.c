// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "vm.h"

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

struct page pages[PHYSTOP/PGSIZE];
struct page *page_lru_head;
int num_free_pages;
int num_lru_pages;
struct spinlock lru_list_lock;
void lru_list_init();
int lru_list_add(uint paddr, pde_t *pgdir, char *vaddr);
int lru_list_delete(uint paddr);

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  lru_list_init();
  stable_init();
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
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
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

/* pa4 */
// swap-out
int
reclaim() {
  release(&kmem.lock);
  acquire(&lru_list_lock);
  if(num_lru_pages == 0)
    return 0;
  pte_t *pte;
  struct page *victim;
  for(victim = page_lru_head; ; victim = victim->next) {
    if((uint)victim->vaddr >= 0x0000 && (uint)victim->vaddr <= 0x2000) { // avoid evict init and sh
      page_lru_head = victim;
      continue;
    }
    pte = walkpgdir(victim->pgdir, victim->vaddr, 0);
    if((char*)P2V(PTE_ADDR(*pte)) < end) { // avoid kfree panic
      page_lru_head = victim;
      continue;
    }
    if(!(*pte & PTE_U)) { // avoid evict no user page 
      page_lru_head = victim;
      continue;
    }
    if(*pte & PTE_A) {
      *pte &= ~PTE_A;
      page_lru_head = victim;
    } 
    else 
      break;
  }
  release(&lru_list_lock);
  uint pa = PTE_ADDR(*pte);
  if(pa == 0)
    return 0;

  int blkno = stable_get_freeblk();

  if(blkno < 0)
    return 0;

  swapwrite(victim->vaddr, blkno);
  lru_list_delete(pa);
  kfree(P2V(pa));
  
  *pte &= PTE_FLAGS(*pte);
  *pte &= ~PTE_P;
  *pte |= blkno << 12;

  return 1;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

try_again:
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(!r){
    if(reclaim())
      goto try_again;
    else{
      cprintf("kalloc out of memory\n");
      return 0;
    }
  }
  if(r)
    kmem.freelist = r->next;
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

/* pa4 */
// lru list management

void 
lru_list_init() {
  page_lru_head = 0;
  num_free_pages = PHYSTOP / PGSIZE;
  num_lru_pages = 0;
  initlock(&lru_list_lock, "lru_list");
}

// success 0, fail -1
int
lru_list_add(uint paddr, pde_t *pgdir, char *vaddr) {
  int idx = paddr / PGSIZE;
  struct page *p;

  acquire(&lru_list_lock);
  if(num_free_pages == 0){
    release(&lru_list_lock);
    return -1;
  }
  p = &pages[idx];

  if (p->next != 0 || p->prev != 0) {
    release(&lru_list_lock);
    return -1;
  }

  p->pgdir = pgdir;
  p->vaddr = vaddr;
  if(page_lru_head) {
    struct page* tail = page_lru_head->prev;
    tail->next->prev = p;
    tail->next = p;
    p->prev = tail;
    p->next = page_lru_head;
  } else {
    page_lru_head = p;
    p->next = p;
    p->prev = p;
  }
  num_lru_pages++;
  num_free_pages--;
  release(&lru_list_lock);

  return 0;
}

// success 0, fail -1
int
lru_list_delete(uint paddr) {
  int idx = paddr / PGSIZE;
  struct page *p;

  acquire(&lru_list_lock);
  p = &pages[idx];

  if (p->next == 0 || p->prev == 0) {
    release(&lru_list_lock);
    return -1;
  }

  if(p->next == p && p->prev == p) {
    page_lru_head = 0;
  } else {
    p->prev->next = p->next;
    p->next->prev = p->prev;
    if(page_lru_head == p) {
      page_lru_head = p->next;
    }
  }
  p->next = 0;
  p->prev = 0;
  num_lru_pages--;
  num_free_pages++;
  release(&lru_list_lock);

  return 0;
}