#include <write/write.h>
#include <kernel/user.h>
#include <kernel/process.h>

void user_init(void)
{
}

uid_t getuid(void)
{
    struct process *process = process_current();

    if (process == 0)
        return UID_ROOT;

    return process->uid;
}

int setuid(uid_t uid)
{
    struct process *process = process_current();

    if (process == 0)
        return -1;

    if (process->uid != UID_ROOT &&
        process->uid != UID_SROOT) {
        k_print("Only root can change the current UID!\n");
        return -1;
    }

    process->uid = uid;
    return 0;
}