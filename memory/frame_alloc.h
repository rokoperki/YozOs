#ifndef FRAME_ALLOC
#define FRAME_ALLOC

#include "../cpu/types.h"

#define FRAME_SIZE 0x1000 // 4Kib
#define NUM_FRAMES 32768
#define align_up(x, a) (((x) + (a) - 1) & ~((a) - 1))

void init_frames();
u32 alloc_frame();
void free_frame(u32 addr);

#endif
