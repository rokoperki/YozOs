
#include "task.h"
#include "../drivers/screen.h"
#include "../kernel/string.h"
#include "../memory/frame_alloc.h"

static task_t main_task; // boot/kernel thread ]
static task_t idle_task;
static task_t *current;
static task_t *task_list;
static u32 next_task_id;
static int scheduler_ready;

static int task_test_started;

static task_t task_table[MAX_TASKS];
static task_t *task_a;
static task_t *task_b;

static void idle_thread() {
  while (1) {
    asm volatile("sti; hlt");
  }
}

static const char *task_state_name(task_state_t state) {
  if (state == TASK_READY)
    return "READY";
  if (state == TASK_RUNNING)
    return "RUNNING";
  if (state == TASK_BLOCKED)
    return "BLOCKED";
  if (state == TASK_EXITED)
    return "EXITED";
  if (state == TASK_UNUSED)
    return "UNUSED";
  return "?";
}

void task_exit(void) {
  current->state = TASK_EXITED;
  schedule();

  while (1) {
    asm volatile("hlt");
  }
}

void scheduler_init() {
  main_task.id = next_task_id++;
  main_task.name = "main";
  main_task.state = TASK_RUNNING;
  main_task.esp = 0;
  main_task.next = &main_task;
  current = &main_task;
  task_list = &main_task;

  create_task(&idle_task, "idle", idle_thread);
  add_task(&idle_task);

  scheduler_ready = 1;
}

void add_task(task_t *t) {
  task_t *last = task_list;

  while (last->next != task_list) {
    last = last->next;
  }

  t->next = task_list;
  last->next = t;
}

u32 task_get_id(task_t *t) { return t ? t->id : 0; }

void create_task(task_t *t, const char *name, void (*entry)()) {
  u32 *stack = (u32 *)(alloc_frame() + FRAME_SIZE);
  *(--stack) = (u32)task_exit;
  *(--stack) = (u32)entry; // ret addr
  *(--stack) = (u32)task_trampoline;
  *(--stack) = 0; // ebp
  *(--stack) = 0; // ebx
  *(--stack) = 0; // esi
  *(--stack) = 0; // edi
  t->esp = (u32)stack;
  t->id = next_task_id++;
  t->name = name;
  t->state = TASK_READY;
  t->next = 0;
}

void schedule() {
  if (!scheduler_ready || !current)
    return;

  task_t *prev = current;
  task_t *next = current->next;

  while (next != current && next->state != TASK_READY) {
    next = next->next;
  }

  if (next->state != TASK_READY)
    return;

  if (prev->state == TASK_RUNNING) {
    prev->state = TASK_READY;
  }

  next->state = TASK_RUNNING;
  current = next;

  if (prev != next) {
    switch_context(&prev->esp, next->esp);
  }
}

task_state_t task_get_state(task_t *t) { return t->state; }

void thread_a() {
  for (int i = 0; i < 100; i++) {
    print("A");
    schedule();
  }
}

void thread_b() {
  for (int i = 0; i < 100; i++) {
    print("B");
    schedule();
  }
}

void task_remove(task_t *target) {
  if (!task_list || !target || target == task_list || target == current)
    return;

  task_t *prev = task_list;
  task_t *t = task_list->next;

  while (t != task_list) {
    if (t == target) {
      prev->next = t->next;
      t->next = 0;
      t->state = TASK_UNUSED;
      t->name = "unused";
      t->esp = 0;
      return;
    }

    prev = t;
    t = t->next;
  }
}

void test_task() {
  if (task_test_started)
    return;

  task_t *a = spawn_task("task_a", thread_a);
  task_t *b = spawn_task("task_b", thread_b);
  if (!a || !b) {
    println("no task slots");
    if (a)
      task_remove(a);
    if (b)
      task_remove(b);
    return;
  }

  task_test_started = 1;
  task_a = a;
  task_b = b;

  schedule();
}

static int task_from_table(task_t *t) {
  return t >= &task_table[0] && t < &task_table[MAX_TASKS];
}

void task_reap_exited(void) {
  if (!task_list)
    return;

  task_t *prev = task_list;
  task_t *t = task_list->next;

  while (t != task_list) {
    if (t != &idle_task && t != current && t->state == TASK_EXITED) {
      prev->next = t->next;

      if (t == task_a)
        task_a = 0;
      if (t == task_b)
        task_b = 0;
      if (!task_a && !task_b)
        task_test_started = 0;

      t->next = 0;
      if (task_from_table(t)) {
        t->state = TASK_UNUSED;
        t->name = "unused";
        t->esp = 0;
      }

      t = prev->next;
      continue;
    }

    prev = t;
    t = t->next;
  }
}

void task_dump() {
  if (!task_list)
    return;

  task_t *t = task_list;

  do {
    char buf[16];
    int_to_ascii(t->id, buf);
    print(buf);
    print(" ");

    print(t->name);
    print(" ");

    println(task_state_name(t->state));

    t = t->next;

  } while (t != task_list);
}

task_t *task_alloc() {
  for (int i = 0; i < MAX_TASKS; i++) {
    if (task_table[i].state == TASK_UNUSED) {
      task_table[i].state = TASK_BLOCKED;
      task_table[i].name = "reserved";
      task_table[i].next = 0;
      return &task_table[i];
    }
  }

  return 0;
}

task_t *spawn_task(const char *name, void (*entry)()) {
  task_t *task = task_alloc();
  if (!task)
    return 0;

  create_task(task, name, entry);
  add_task(task);
  return task;
}
