#include "types.h"
#include "heap.h"

extern void* __heap_end__;

void heapRepair(struct heap_chunk* chunk_to_remove) {
  //we want to repair the fake_free_chunk which is just next to chunk_to_remove
  struct heap_chunk* chunk_to_repair =
    (struct heap_chunk*)(chunk_to_remove->data + chunk_to_remove->size);

  //because chunk_to_remove->next = chunk_to_repair
  chunk_to_remove->prev->next = chunk_to_repair;
  chunk_to_repair->prev = chunk_to_remove->prev;
  //this is the last free chunk
  chunk_to_repair->next = NULL;
  chunk_to_repair->magic = 0x4652;
  //there's no allocated chunk after it
  chunk_to_repair->size = (struct heap_chunk*)__heap_end__ - (chunk_to_repair + 1);
}
