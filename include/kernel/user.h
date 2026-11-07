#ifndef KERNEL_USER_H
#define KERNEL_USER_H

#include <stdint.h>

typedef uint32_t uid_t;

/*
If you want to know what each type of user can do, this is the comment
to look at:

UID -1000:
 * Can do anything

UID 0:
 * Can do almost anything

UID 1000:
 * Cannot access other user profiles
 * Cannot write/read system files or root files
 * Cannot write to root permissioned folders

*/

#define UID_SROOT ((uid_t) -1000) // Super Root
#define UID_ROOT 0u // Root User
#define UID_USER 1000u // Regular User

struct user {
    uid_t uid;
};

void user_init(void);

uid_t getuid(void);
int setuid(uid_t uid);

#endif