#include "frame_alloc.h"
#include "../cpu/types.h"
#include "../kernel/mem.h"
#include "memory_map.h"
static u32 frame_bitmap[1024];

static void set(u32 f) { frame_bitmap[f >> 5] |= (1 << (f & 31)); }
static void clear(u32 f) { frame_bitmap[f >> 5] &= ~(1 << (f & 31)); }
static int test(u32 f) { return frame_bitmap[f >> 5] & (1 << (f & 31)); }

void init_frames() {
  memory_set((u8 *)frame_bitmap, 0xFF, sizeof(frame_bitmap));

  int count = *(u16 *)MMAP_ENT_NUM;
  e820_entry_t *entries = (e820_entry_t *)MMAP_ENT_START;

  for (int i = 0; i < count; i++) {
    if (entries[i].type == USABLE && entries[i].base >= 0x100000) {
      u32 start = align_up(entries[i].base, FRAME_SIZE) >> 12;
      u32 end = (entries[i].base + entries[i].length) >> 12;
      if (end > 32768)
        end = 32768;
      for (u32 f = start; f < end; f++) {
        clear(f);
      }
    }
  }
}

u32 alloc_frame() {
  for (int f = 0; f < NUM_FRAMES; f++) {
    if (!test(f)) {
      set(f);
      return f << 12;
    }
  }
  return 0;
}

void free_frame(u32 addr) { clear(addr >> 12); }
