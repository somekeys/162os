#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#define pid_t tid_t
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void push_arg(char* f_name, char** save, void** esp);
struct process{
    pid_t pid;
    int exit_code;
    struct list_elem elem;
    struct semaphore  wait;
    bool waiting;
    struct thread* t;
};
#endif /* userprog/process.h */
