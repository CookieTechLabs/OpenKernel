#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <stdint.h>
#include <kernel/user.h>

#define PROCESS_STATE_UNUSED    0
#define PROCESS_STATE_READY     1
#define PROCESS_STATE_RUNNING   2
#define PROCESS_STATE_BLOCKED   3
#define PROCESS_STATE_ZOMBIE    4

struct process {
    uint32_t pid;
    uint32_t parent_pid;

    uid_t uid;

    uint32_t state;
    int32_t exit_status;

    uint32_t kernel_stack;
    uint32_t kernel_stack_size;
    uint32_t *interrupt_frame;
    uint32_t fork_child;

    uint32_t context[6];

    struct process *next;
};

void process_system_init(void);
uint32_t process_current_pid(void);
uint32_t process_enter_program(void);
void process_leave_program(uint32_t previous_pid);
int32_t process_execute(int32_t (*entry)(void), int returns_status);
void process_exit_current(int32_t status);
int32_t process_load_oso(const char *path);
int32_t process_fork_current(void);
void process_interrupt_enter(uint32_t *frame);
uint32_t *process_interrupt_return(uint32_t *frame, uint32_t syscall_number);

struct process *process_find(uint32_t pid);
struct process *process_alloc(void);
struct process *process_current(void);

#endif
