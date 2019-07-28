#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#define KILL() thread_exit()
#define CHECK_GET(PTR,OFFSET,BYTEOFF) (get_user((uint8_t *) (((uint32_t*) PTR)+(OFFSET)) + BYTEOFF) != -1)
static struct lock file_lock;

static void syscall_handler (struct intr_frame *);

/* Reads a byte at user virtual address UADDR.
 *    UADDR must be below PHYS_BASE.
 *       Returns the byte value if successful, -1 if a segfault
 *          occurred. */
static int
get_user (const uint8_t *uaddr)
{
      if ((uint32_t) uaddr >= (uint32_t) PHYS_BASE)
              return -1;

        int result;
          asm ("movl $1f, %0; movzbl %1, %0; 1:"
                         : "=&a" (result) : "m" (*uaddr));

            return result;
}

/* Writes BYTE to user address UDST.
 *    UDST must be below PHYS_BASE.
 *       Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
      if ((uint32_t) udst >= (uint32_t) PHYS_BASE)
              return false;

        int error_code;
          asm ("movl $1f, %0; movb %b2, %1; 1:"
                         : "=&a" (error_code), "=m" (*udst) : "q" (byte));
            return error_code != -1;
}

inline static int 
checked_get(void* ptr, int offset){
    if(!CHECK_GET(ptr,offset,3)) KILL();
    return *((uint32_t*)ptr + offset);
}

static bool 
check_string(char* str){
    while(CHECK_GET(str, 0,0)){
        if(str[0] == '\0') return true; str++;
    }
   
    return false;
}

static int 
sys_wait (tid_t pid){
    return process_wait(pid);
}
static void 
sys_exit(int err_code){
    thread_current() ->exit_code = err_code;
#ifdef USERPROG

    if(thread_current()->p){
        thread_current () -> p -> exit_code = err_code;
    }
#endif
    thread_exit();
}

static inline int 
sys_practice(int a){
    return a+1;
}

static inline void 
sys_halt(){
    shutdown_power_off();
}

static int sys_exec(const char* cmd_line){
    if(!check_string(cmd_line)) KILL();
    return process_execute(cmd_line);
}
void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);
  switch(checked_get(args,0))
  {
    case SYS_EXIT:
          sys_exit(checked_get(args,1));
          break;
    case SYS_PRACTICE:
          f->eax = sys_practice(checked_get(args,1));
          break;
    case SYS_HALT:
          sys_halt();
          break;
    case SYS_EXEC:
          f->eax = sys_exec((char*)checked_get(args,1));
    case SYS_WAIT:
          f->eax = sys_wait((tid_t)checked_get(args,1));
    case SYS_OPEN:
    default:
          printf("Calling system call: number %d\n", checked_get(args,0));
        
        
  }
  /*
         *f->eax = args[1];
         *printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
         *thread_exit();
         */
}
