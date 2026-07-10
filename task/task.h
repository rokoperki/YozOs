#ifndef TASK
#define TASK

#include "../cpu/types.h"
#include "../memory/paging.h"

typedef enum {
  TASK_UNUSED,
  TASK_READY,
  TASK_RUNNING,
  TASK_BLOCKED,
  TASK_EXITED,
} task_state_t;

struct user_process;

typedef struct task {
  u32 id;
  const char *name;
  task_state_t state;
  u32 esp; // saved stack pointer
  struct user_process *user_process;
  address_space_t *address_space;
  struct task *next; // round-robin ring
} task_t;

void create_task(task_t *t, const char *name, void (*entry)());

extern void switch_context(u32 *old_esp, u32 new_esp);
extern void task_trampoline();

void test_task();
void schedule();

void scheduler_init(void);
void add_task(task_t *t);
void task_exit(void);
void task_dump(void);
void task_reap_exited(void);
u32 task_get_id(task_t *t);
int task_kill(task_t *task);

task_state_t task_get_state(task_t *t);

#define MAX_TASKS 16
task_t *task_alloc(void);
task_t *spawn_task(const char *name, void (*entry)());
void task_remove(task_t *target);

void task_set_user_process(task_t *task, struct user_process *process);
struct user_process *task_get_user_process(task_t *task);
void task_set_address_space(task_t *task, address_space_t *space);
address_space_t *task_get_address_space(task_t *task);

task_t *task_current(void);

#endif
