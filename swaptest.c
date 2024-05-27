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
#define NUM_PAGES 20000

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

  base = sbrk(NUM_PAGES * PGSIZE);
  if (base == (char*)-1) {
    printf(1, "sbrk failed\n");
    exit();
  }

  touch_pages(base, NUM_PAGES);

  for (i = 0; i < NUM_PAGES; i++) {
    base[i * PGSIZE] = i;
  }
}

int
main(void)
{
  int a, b; //a swapread, b swapwrite
  int c; //c lrustat
  int pid;
  pid = fork();
  if(pid==0){
      forkfn();
  }
  else{
      wait();
      printf(1, "swaptest completed\n");
  }
  lrustat(&c);
  swapstat(&a, &b);
  printf(1, "swapstat: %d %d\n", a, b);
  printf(1, "lrustat: %d\n", c);
  pid = fork();
  if(pid==0){
      forkfn();
  }
  else{
      wait();
      printf(1, "swaptest completed\n");
  }
  lrustat(&c);
  swapstat(&a, &b);
  printf(1, "swapstat: %d %d\n", a, b);
  printf(1, "lrustat: %d\n", c);
  exit();
}