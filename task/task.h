#ifndef TASK
#define TASK

#include "../cpu/types.h"

typedef struct task {
  u32 esp;           // saved stack pointer
  struct task *next; // round-robin ring
} task_t;

void create_task(task_t *t, void (*entry)());

extern void switch_context(u32 *old_esp, u32 new_esp);

void test_task();
void yield();

#endif
