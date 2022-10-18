// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define LOCK_NAME_LEN 16

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

char kmem_lock_name[NCPU][LOCK_NAME_LEN];

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    // Call initlock for each of your locks, and pass a name that starts with "kmem".
    char *name = kmem_lock_name[i];
    snprintf(name, LOCK_NAME_LEN - 1, "kmem_cpu_%d", i);
    initlock(&kmem[i].lock, name);
  }
  // Let freerange give all free memory to the CPU running freerange.
  freerange(end, (void *)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  push_off();
  int cpu = cpuid();
  pop_off();
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_cpu(p, cpu);
}

// Free the page of physical memory pointed at by v,
// add the page to CPU's list with given cpuid.
void
kfree_cpu(void *pa, int cpuid)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpuid].lock);
  r->next = kmem[cpuid].freelist;
  kmem[cpuid].freelist = r;
  release(&kmem[cpuid].lock);
}

// Free the page of physical memory pointed at by pa,
// add the page to current CPU's free list
void kfree(void *pa) { 
  push_off();
  int cpu = cpuid();
  pop_off();
  kfree_cpu(pa, cpu);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu = cpuid();
  pop_off();
  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;
  if(r) { // if current cpu freelist is not empty
    kmem[cpu].freelist = r->next;
    release(&kmem[cpu].lock);
  } else { //else steal from other cpu's freelist
    release(&kmem[cpu].lock);
    for (int i = 0; i < NCPU; i++) {
      // traverse each cpu's freelist except current one
      if (i == cpu) {
        continue;
      }
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r) { 
        // if node found, no need to search for next cpu's freelist, just break
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
