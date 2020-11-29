#pragma once

struct heap_chunk {
  u16 magic;
  u16 region_start_offset;
  u32 size;
  struct heap_chunk* prev;
  struct heap_chunk* next;
  u8 data[0];
};

extern void* heapAllocate(u32 size);
void heapRepair(struct heap_chunk* chunk_to_remove);
