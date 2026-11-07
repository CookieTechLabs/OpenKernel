#include <kernel/process.h>
#include <kernel/syscall.h>
#include <kernel/user.h>
#include <kernel.h>
#include <memory/page.h>
#include <kernel/vdso.h>

static uint32_t current_pid;
static uint32_t next_pid;
static uint32_t execution_context[6];
static volatile int execution_context_active;
static volatile int inside_syscall;

#define PROCESS_MAX 512u
#define STACK_PAGE_COUNT 4u

static struct process process_table[PROCESS_MAX];
static struct process *current_process;

struct process *process_current(void)
{
    return current_process;
}

extern int32_t process_context_save(uint32_t *context);
extern void process_context_restore(uint32_t *context, int32_t status)
    __attribute__((noreturn));
extern uint8_t stack_bottom __attribute__((weak));
extern uint8_t stack_top __attribute__((weak));

static void process_set_boot_stack(struct process *process)
{
    uintptr_t bottom = (uintptr_t)&stack_bottom;
    uintptr_t top = (uintptr_t)&stack_top;

    if (bottom == 0 || top <= bottom) {
        process->kernel_stack = 0;
        process->kernel_stack_size = 0;
        return;
    }

    process->kernel_stack = (uint32_t)bottom;
    process->kernel_stack_size = (uint32_t)(top - bottom);
}

static struct process *process_find_slot(uint32_t pid)
{
    for (uint32_t index = 0; index < PROCESS_MAX; index++) {
        if (process_table[index].state != PROCESS_STATE_UNUSED &&
            process_table[index].pid == pid) {
            return &process_table[index];
        }
    }

    return 0;
}

static void reap_zombie_stacks(void)
{
    for (uint32_t index = 0; index < PROCESS_MAX; index++) {
        struct process *process = &process_table[index];

        if (process != current_process &&
            process->state == PROCESS_STATE_ZOMBIE &&
            process->kernel_stack != 0 &&
            process->fork_child) {

            uint32_t pages =
                process->kernel_stack_size / MEMORY_PAGE_SIZE;

            if (memory_free_pages_at(
                    (void *)(uintptr_t)process->kernel_stack,
                    pages) == 0) {

                process->kernel_stack = 0;
                process->kernel_stack_size = 0;
                process->state = PROCESS_STATE_UNUSED;
                process->pid = 0;
                process->parent_pid = 0;
                process->uid = UID_ROOT;
                process->interrupt_frame = 0;
                process->fork_child = 0;
                process->exit_status = 0;
                process->next = 0;
            }
        }
    }
}

struct process *process_find(uint32_t pid)
{
    return process_find_slot(pid);
}

struct process *process_alloc(void)
{
    for (uint32_t index = 1; index < PROCESS_MAX; index++) {
        if (process_table[index].state == PROCESS_STATE_UNUSED) {
            return &process_table[index];
        }
    }

    return 0;
}

void process_system_init(void)
{
    current_pid = 0;
    vdso_set_current_pid(current_pid);

    next_pid = 1;
    current_process = &process_table[0];
    inside_syscall = 0;

    execution_context_active = 0;

    for (uint32_t index = 0; index < PROCESS_MAX; index++) {
        process_table[index].pid = 0;
        process_table[index].parent_pid = 0;
        process_table[index].uid = UID_ROOT;
        process_table[index].state = PROCESS_STATE_UNUSED;
        process_table[index].exit_status = 0;
        process_table[index].kernel_stack = 0;
        process_table[index].kernel_stack_size = 0;
        process_table[index].interrupt_frame = 0;
        process_table[index].fork_child = 0;

        for (uint32_t word = 0; word < 6; word++) {
            process_table[index].context[word] = 0;
        }

        process_table[index].next = 0;
    }

    process_table[0].pid = 0;
    process_table[0].uid = UID_SROOT;
    process_table[0].state = PROCESS_STATE_RUNNING;

    process_set_boot_stack(&process_table[0]);
}

uint32_t process_current_pid(void)
{
    return current_pid;
}

uint32_t process_enter_program(void)
{
    uint32_t previous_pid = current_pid;
    struct process *parent = current_process;
    struct process *process = process_alloc();

    if (process == 0) {
        return previous_pid;
    }

    if (parent != 0 &&
        parent->state == PROCESS_STATE_RUNNING) {
        parent->state = PROCESS_STATE_READY;
    }

    current_pid = next_pid++;

    vdso_set_current_pid(current_pid);

    if (next_pid == 0) {
        next_pid = 1;
    }

    process->pid = current_pid;
    process->parent_pid = previous_pid;
    process->uid = parent != 0 ? parent->uid : UID_ROOT;
    process->state = PROCESS_STATE_RUNNING;
    process->exit_status = 0;
    process_set_boot_stack(process);
    process->interrupt_frame = 0;
    process->fork_child = 0;
    process->next = 0;

    current_process = process;

    return previous_pid;
}

void process_leave_program(uint32_t previous_pid)
{
    if (current_process != 0 &&
        current_process->pid != previous_pid &&
        !current_process->fork_child) {

        current_process->state = PROCESS_STATE_UNUSED;
        current_process->uid = UID_ROOT;
        current_process->pid = 0;
        current_process->parent_pid = 0;
        current_process->exit_status = 0;
        current_process->interrupt_frame = 0;
        current_process->fork_child = 0;
        current_process->next = 0;
    }

    current_pid = previous_pid;
    vdso_set_current_pid(current_pid);

    current_process = process_find_slot(previous_pid);

    if (current_process == 0) {
        current_process = &process_table[0];
    }

    current_process->state = PROCESS_STATE_RUNNING;
}

int32_t process_execute(int32_t (*entry)(void), int returns_status)
{
    int32_t status;

    execution_context_active = 1;

    status = process_context_save(execution_context);

    if (status == 0 && execution_context_active) {
        if (returns_status) {
            status = entry();
        } else {
            ((void (*)(void))entry)();
            status = 0;
        }

        process_exit_current(status);
    }

    execution_context_active = 0;

    return (int32_t)(uint8_t)status;
}

int32_t process_fork_current(void)
{
    struct process *parent = current_process;
    struct process *child;

    uint32_t source_base;
    uint32_t stack_size;
    uint8_t *destination;
    uint8_t *source;
    uint32_t frame_offset;

    if (parent == 0 ||
        parent->interrupt_frame == 0 ||
        parent->kernel_stack == 0 ||
        parent->kernel_stack_size == 0) {
        return -OK_ENOSYS;
    }

    child = process_alloc();

    if (child == 0) {
        return -OK_ENOMEM;
    }

    stack_size = parent->kernel_stack_size;

    if (stack_size != STACK_PAGE_COUNT * MEMORY_PAGE_SIZE) {
        return -OK_ENOSYS;
    }

    destination = (uint8_t *)memory_alloc_pages(STACK_PAGE_COUNT);

    if (destination == 0) {
        return -OK_ENOMEM;
    }

    source_base = parent->kernel_stack;
    source = (uint8_t *)(uintptr_t)source_base;

    frame_offset =
        (uint32_t)((uintptr_t)parent->interrupt_frame - source_base);

    if (frame_offset >
        stack_size - sizeof(struct kernel_panic_registers)) {

        memory_free_pages_at(destination, STACK_PAGE_COUNT);
        return -OK_EFAULT;
    }

    for (uint32_t offset = 0; offset < stack_size; offset++) {
        destination[offset] = source[offset];
    }

    for (uint32_t offset = 0;
         offset + sizeof(uint32_t) <= stack_size;
         offset += sizeof(uint32_t)) {

        uint32_t value = *(uint32_t *)(destination + offset);

        if (value >= source_base &&
            value < source_base + stack_size) {

            *(uint32_t *)(destination + offset) =
                value - source_base +
                (uint32_t)(uintptr_t)destination;
        }
    }

    child->pid = next_pid++;

    if (next_pid == 0) {
        next_pid = 1;
    }

    child->parent_pid = parent->pid;
    child->uid = parent->uid;
    child->state = PROCESS_STATE_READY;
    child->exit_status = 0;
    child->kernel_stack = (uint32_t)(uintptr_t)destination;
    child->kernel_stack_size = stack_size;

    child->interrupt_frame =
        (uint32_t *)(destination + frame_offset);

    child->fork_child = 1;
    child->next = 0;

    struct kernel_panic_registers *parent_frame =
        (struct kernel_panic_registers *)parent->interrupt_frame;

    struct kernel_panic_registers *child_frame =
        (struct kernel_panic_registers *)child->interrupt_frame;

    parent_frame->eax = child->pid;
    child_frame->eax = 0;

    return (int32_t)child->pid;
}

void process_interrupt_enter(uint32_t *frame)
{
    reap_zombie_stacks();

    inside_syscall = 1;

    if (current_process != 0) {
        current_process->interrupt_frame = frame;
    }
}

uint32_t *process_interrupt_return(
    uint32_t *frame,
    uint32_t syscall_number)
{
    struct process *next = 0;
    int should_schedule;

    inside_syscall = 0;

    if (current_process == 0) {
        return frame;
    }

    current_process->interrupt_frame = frame;

    should_schedule =
        current_process->state == PROCESS_STATE_ZOMBIE ||
        syscall_number == OK_SYS_YIELD;

    if (!should_schedule) {
        return frame;
    }

    if (current_process->state == PROCESS_STATE_RUNNING) {
        current_process->state = PROCESS_STATE_READY;
    }

    for (uint32_t index = 0; index < PROCESS_MAX; index++) {
        if (&process_table[index] != current_process &&
            process_table[index].state == PROCESS_STATE_READY &&
            process_table[index].interrupt_frame != 0) {

            next = &process_table[index];
            break;
        }
    }

    if (next == 0) {
        if (current_process->state == PROCESS_STATE_READY) {
            current_process->state = PROCESS_STATE_RUNNING;
        }

        return frame;
    }

    current_process = next;
    current_pid = next->pid;

    vdso_set_current_pid(current_pid);

    next->state = PROCESS_STATE_RUNNING;

    return next->interrupt_frame;
}

void process_exit_current(int32_t status)
{
    if (current_process != 0 &&
        current_process->fork_child) {

        current_process->exit_status = (int32_t)(uint8_t)status;
        current_process->state = PROCESS_STATE_ZOMBIE;

        if (!inside_syscall) {
            __asm__ volatile (
                "int $0x80"
                :
                : "a" (OK_SYS_YIELD)
                : "memory", "cc"
            );

            for (;;) {
                __asm__ volatile ("pause");
            }
        }

        return;
    }

    if (current_pid == 1) {
        panic("PID 1 (init) exited!");
    }

    if (execution_context_active) {
        execution_context_active = 0;

        process_context_restore(
            execution_context,
            (int32_t)(uint8_t)status
        );
    }
}