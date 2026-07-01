
#include "task.h"
#include "../drivers/screen.h"
#include "../memory/frame_alloc.h"

static task_t main_task; // boot/kernel thread ]
static task_t *current;
static task_t task_a, task_b;

void create_task(task_t *t, void (*entry)()) {
  u32 *stack = (u32 *)(alloc_frame() + FRAME_SIZE);
  *(--stack) = (u32)entry; // ret addr
  *(--stack) = 0;          // ebp
  *(--stack) = 0;          // ebx
  *(--stack) = 0;          // esi
  *(--stack) = 0;          // edi
  t->esp = (u32)stack;
}

void yield() {
  task_t *prev = current;
  current = current->next;
  switch_context(&prev->esp, current->esp);
}

void thread_a() {
  while (1) {
    print("A");
    yield();
  }
}

void thread_b() {
  while (1) {
    print("B");
    yield();
  }
}

void test_task() {
  create_task(&task_a, thread_a);
  create_task(&task_b, thread_b);

  main_task.next = &task_a;
  task_a.next = &task_b;
  task_b.next = &task_a;

  current = &main_task;
  yield();
}
