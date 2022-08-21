#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 va;
  int page_nums;
  uint64 out_addr;
  // get syscall params
  if(argaddr(0, &va) < 0) {
    return -1;
  }
  if(argint(1, &page_nums) < 0) {
    return -1;
  }
  if(argaddr(2, &out_addr) < 0) {
    return -1;
  }

  // check if the page numbers to scan is valid
  if(page_nums < 0 || page_nums > 64) {
    return -1;
  }

  uint64 bitmask = 0;
  pte_t *pte;
  struct proc *p = myproc();

  for(int i = 0; i < page_nums; i++) {
    // check if va is out of range
    if(va >= MAXVA) {
      return -1;
    }
    // get pte addr by va
    pte = walk(p->pagetable, va, 0);
    if(!pte) {
      return -1;
    }
    // check if accessed
    if(*pte & PTE_A) {
      bitmask |= (1 << i);
      // clear the bit afterwards
      *pte ^= PTE_A;
    }
    // move va to next mem page
    va += PGSIZE;
  }
  
  // copy bitmask to user space
  if(copyout(p->pagetable, out_addr, (char *)&bitmask, sizeof(bitmask)) < 0) {
    return -1;
  }
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
