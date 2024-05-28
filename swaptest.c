#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

#define PGSIZE 4096
#define NUM_PAGES 10000

void
touch_pages(char *base, int num_pages)
{
  int i;
  for (i = 0; i < num_pages; i++) {
    base[i * PGSIZE] = i;
  }
}

void
forkfn() 
{
  char *base;
  int i;
  int a, b; //a swapread, b swapwrite

  base = sbrk(NUM_PAGES * PGSIZE);
  if (base == (char*)-1) {
    printf(1, "sbrk failed\n");
    exit();
  }

  touch_pages(base, NUM_PAGES);

  for (i = 0; i < NUM_PAGES; i++) {
    base[i * PGSIZE] = i;
  }
  swapstat(&a, &b);
  printf(1, "swapstat: %d %d\n", a, b);
}

int
main(void)
{
  int i, pid;
  for(i=0; i<3; i++) {
    pid = fork();
    if(pid==0){
        forkfn();
    }
    else{
        wait();
        printf(1, "swaptest completed\n");
    }
  }
  exit();
}