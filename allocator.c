#include <stddef.h>
#include <stdint.h>

#define HEAP_START    0x40000
#define HEAP_END      0x100000
#define HEAP_SIZE     HEAP_END - HEAP_START

struct block_metadata {
  uint32_t size;  // Only the lower 20-bits are managed.
  int state;

  struct block_metadata* next;
  struct block_metadata* prev;
};

#define HEADER_SIZE         sizeof(struct block_metadata)
#define MIN_REQ_SIZE        HEADER_SIZE
#define MAX_ALIGNED_SIZE    HEAP_SIZE - MIN_REQ_SIZE


// Block states
#define BLOCK_FREE   0
#define BLOCK_INUSE  1
#define BLOCK_UNUSED 2

#define MALLOC_FAILED  (void*)(-1)

// Pointer to the first free chunk.
static struct block_metadata* list_head;

// Initialize the allocator metadata.
void allocator_init(){
  list_head = (struct block_metadata*)(0x40000);

  list_head->size  = HEAP_SIZE;
  list_head->state = BLOCK_UNUSED;
  list_head->next  = NULL;
  list_head->prev  = NULL;
}

// Request memory from the allocator.
void *get_mem(uint32_t bytes){
  uint32_t alinged_bytes = bytes + sizeof(struct block_metadata);
  if (alinged_bytes > MAX_ALIGNED_SIZE){
    return MALLOC_FAILED;
  }

  // Traverse the list.
  for (
    struct block_metadata* block = list_head;
    block != NULL;
    block = block->next
  ){
    if (block->state != BLOCK_INUSE){
      // Split
      if (block->size > (alinged_bytes + HEADER_SIZE)){
        struct block_metadata* remainder = (struct block_metadata*)((char*)(block) + alinged_bytes);

        remainder->size  = block->size - alinged_bytes; 
        remainder->prev  = block;
        remainder->next  = block->next;
        remainder->state = (block->state == BLOCK_FREE)
                           ? BLOCK_FREE
                           : BLOCK_UNUSED;

        if (block->next != NULL){
          (block->next)->prev = remainder;
        }

        block->size = alinged_bytes;
        block->next = remainder;
      }

      // Return
      block->state = BLOCK_INUSE;
      return (char*)(block) + HEADER_SIZE;
    }
  }

  return MALLOC_FAILED;
}

// Return memory to the allocator.
void free_mem(void* mem){
  struct block_metadata* block = (char*)(mem) - HEADER_SIZE;
  block->state = BLOCK_FREE;

  // Backward coalescing
  if (
    block->prev != NULL &&
    (block->prev)->state == BLOCK_FREE
  ){
    (block->prev)->size += block->size;

    // (n-1)->next = (n+1)
    (block->prev)->next = block->next;

    // (n+1)->prev = (n-1)
    (block->next)->prev = block->prev;

    // Update the block
    block = block->prev;
  }

  // Forward coalescing
  if (
    block->next != NULL &&
    (block->next)->state == BLOCK_FREE
  ){
    block->size += (block->next)->size;

    // (n+2)->prev = n
    if ((block->next)->next != NULL){
      ((block->next)->next)->prev = block;
    }

    // (n)->next = (n+2)
    block->next = (block->next)->next;
  }
}
