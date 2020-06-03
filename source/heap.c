#include "types.h"
#include "heap.h"

extern void* __heap_end__;
extern struct heap_chunk fake_free_chunk;
static struct heap_chunk* const chunk_to_repair = &fake_free_chunk;

void heapRepair(struct heap_chunk* chunk_to_remove) {
  //chunk_to_remove->next = chunk_to_repair
  chunk_to_remove->prev->next = chunk_to_repair;
  chunk_to_repair->prev = chunk_to_remove->prev;
  //this is the last free chunk
  chunk_to_repair->next = NULL;
  chunk_to_repair->magic = 0x4652;
  //there's no allocated chunk after it
  chunk_to_repair->size = (struct heap_chunk*)__heap_end__ - (chunk_to_repair + 1);
}
