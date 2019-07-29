#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "lib/kernel/list.h"
void syscall_init (void);

struct fd_map{
    int fd;
    struct list_elem elem;
    struct file* f;
};

#endif /* userprog/syscall.h */
