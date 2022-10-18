// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define LOCK_NAME_LEN 20

struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket hashtable[NBUCKET];
} bcache;

char bucket_lock_name[NBUCKET][LOCK_NAME_LEN];

int hash(uint dev, uint blockno) {
  return (dev + blockno) % NBUCKET;
}

void
binit(void)
{
  struct buf *b;
  struct bucket *bucket;

  initlock(&bcache.lock, "bcache_evict");
  for (int i = 0; i < NBUCKET; i++) {
    char *name = bucket_lock_name[i];
    snprintf(name, LOCK_NAME_LEN - 1, "bcache_bucket_%d", i);
    initlock(&bcache.hashtable[i].lock, name);
  }
  b = bcache.buf;
  for (int i = 0; i < NBUF; i++) {
    initsleeplock(&bcache.buf[i].lock, "buffer");
    bucket = bcache.hashtable + (i % NBUCKET);
    b->next = bucket->head.next;
    bucket->head.next = b;
    b++;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *prev_replace_buf;
  struct bucket *bucket, *prev_bucket, *cur_bucket;

  int index = hash(dev, blockno);
  bucket = bcache.hashtable + index;

  // Is the block already cached?
  acquire(&bucket->lock);
  for (b = bucket->head.next; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucket->lock);

  // Acquire evict-lock, serialize the evict action
  acquire(&bcache.lock);

  // Check on current bucket again.
  // If another eviction happened between release of bucket-lock and acquire of evict-lock
  // There is chance that block has cached in current bucket but the process is unaware. 
  // Without the check, blocks with same blockno could exist in the bucket.
  acquire(&bucket->lock);
  for (b = bucket->head.next; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucket->lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // Traverse each bucket and each buffer, pick the least recently used buffer
  prev_replace_buf = (void*)0; // the prev node of the buf to evict
  prev_bucket = (void*)0; // prev locked bucket, null initially
  for (cur_bucket = bcache.hashtable; cur_bucket < bcache.hashtable + NBUCKET; cur_bucket++) {
    acquire(&cur_bucket->lock);
    int found = 0;
    for (b = &cur_bucket->head; b->next; b = b->next) {
      if (b->next->refcnt == 0 && 
      (!prev_replace_buf || b->next->timestamp < prev_replace_buf->next->timestamp)) {
        found = 1;
        prev_replace_buf = b;
      }
    }
    if (!found) {
      release(&cur_bucket->lock);
    } else { 
      // if smaller time stamp found in the bucket, consider releasing prev holding bucket,
      // which is no longer needed
      if (prev_bucket) {
        release(&prev_bucket->lock);
      }
      prev_bucket = cur_bucket;
    }
  }
  
  if (!prev_replace_buf) {
    panic("bget: no buffers");
  }
  b = prev_replace_buf->next;

  if (prev_bucket != bucket) {
    // Delete the buf to evict from its bucket, 
    prev_replace_buf->next = b->next;
    release(&prev_bucket->lock);
    // Add the buf to bucket
    acquire(&bucket->lock);
    b->next = bucket->head.next;
    bucket->head.next = b;
  }

  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bucket->lock);
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer and update the timestamp
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int index = hash(b->dev, b->blockno);
  acquire(&bcache.hashtable[index].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->timestamp = ticks;
  }
  release(&bcache.hashtable[index].lock);
}

void
bpin(struct buf *b) {
  int index = hash(b->dev, b->blockno);
  acquire(&bcache.hashtable[index].lock);
  b->refcnt++;
  release(&bcache.hashtable[index].lock);
}

void
bunpin(struct buf *b) {
  int index = hash(b->dev, b->blockno);
  acquire(&bcache.hashtable[index].lock);
  b->refcnt--;
  release(&bcache.hashtable[index].lock);
}


