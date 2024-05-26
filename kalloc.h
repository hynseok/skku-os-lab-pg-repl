#ifndef PHYSTOP
#define PHYSTOP 0xE000000
#endif
#ifndef PGSIZE
#define PGSIZE 4096
#endif
#ifndef TYPE_H
#define TYPE_H
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
#endif

extern struct page pages[PHYSTOP/PGSIZE];
extern struct page *page_lru_head;
extern int num_free_pages;
extern int num_lru_pages;
extern struct spinlock lru_list_lock;
void lru_list_init();
int lru_list_add(uint paddr, pde_t *pgdir, char *vaddr);
int lru_list_delete(uint paddr);
