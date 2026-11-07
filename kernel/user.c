#include <write/write.h>
#include <kernel/user.h>

static uid_t current_uid = UID_ROOT;

void user_init(void)
{
    current_uid = UID_ROOT;
}

uid_t getuid(void)
{
    return current_uid;
}

int setuid(uid_t uid)
{
    /* Only root can change the current UID. */
    if (current_uid != UID_ROOT && current_uid != UID_SROOT)
        return -1;

    current_uid = uid;
    return 0;
}