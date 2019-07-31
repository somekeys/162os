#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#define KILL() sys_exit(-1)
#define CHECK_GET(PTR,OFFSET,BYTEOFF) (get_user((uint8_t *) (((uint32_t*) PTR)+(OFFSET)) + BYTEOFF) != -1)
#define BUF_MAX 512
static struct lock file_lock;
static void sys_exit(int err_code);
static void syscall_handler (struct intr_frame *);

void 
close_all_files(struct list* fl){
    lock_acquire(&file_lock);
   while(!list_empty(fl)){
       struct list_elem* e = list_pop_front(fl);
       struct fd_map* fm = list_entry(e, struct fd_map, elem);
       file_close(fm->f);
       free(fm);
   }
   lock_release(&file_lock);
}
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
static bool 
check_buff(void* buff, int len){
    do{
        if(len<=0) return true;
        len --;
    }while(CHECK_GET(buff, 0,len));
   
    return false;
}
static bool
sys_create(char* name, unsigned initial_size){
    if(!check_string(name)) KILL();
    bool s;
    
    lock_acquire(&file_lock);
    s = filesys_create(name, initial_size);
    lock_release(&file_lock);

    return s;
}

static bool 
sys_remove(char* name){
    if(!check_string(name)) KILL();
    bool s;
    
    lock_acquire(&file_lock);
    s = filesys_remove(name);
    lock_release(&file_lock);

    return s;
}

static struct fd_map*
get_fdmap(int fd){
    struct list_elem *e;
    struct list* l = &(thread_current()->fd_map_list);
     
    for(e = list_begin(l);e != list_end(l); e = list_next(e)){
        struct fd_map* fm = list_entry(e, struct fd_map, elem);
        if(fm->fd == fd) return fm;
    }
    return NULL;

}
static int 
sys_open(char* file){
    if(!check_string(file)) KILL();
    int fd = -1;
    lock_acquire(&file_lock);
    //try open file
    struct file *f =filesys_open(file);
    //if sucess
    if(f){
       fd = 1;
       while(get_fdmap(++fd));
       struct fd_map* fm = malloc(sizeof(struct fd_map));
       fm->f =f;
       fm->fd = fd;
       list_push_front(&thread_current()->fd_map_list, &fm->elem);
    }
    lock_release(&file_lock);

    return fd;
}

static int 
sys_filesize(int fd){
    lock_acquire(&file_lock);
    int size = -1;
    struct fd_map* fm = get_fdmap(fd);
    if(fm){
        size = file_length(fm->f);
    }
    lock_release(&file_lock);
    return size;
}

static int 
sys_write(int fd, void* buff,unsigned size){
    if(!check_buff(buff,size)) KILL();

    if(fd == STDOUT_FILENO){
        int len =size;
        char* buffer =  buff;
        while(len > BUF_MAX){
            putbuf((const char*)buffer, BUF_MAX);
            len -= BUF_MAX;
            buffer += BUF_MAX;
        }
        
        putbuf((const char*)buffer, len);
        return size;
    } 

    lock_acquire(&file_lock);
    int ret = -1;
    struct fd_map* fm = get_fdmap(fd);
    if(fm){
        ret = file_write(fm->f, buff, size);
    }
    lock_release(&file_lock);
    if(ret==-1) KILL();
    return ret;
    
}

static int 
sys_read(int fd, void* buff, unsigned size){
    if(!check_buff(buff,size)) KILL();
    size_t i = 0;
    struct fd_map *fm;
    int ret = -1;

    if (fd == STDIN_FILENO)
        {
         while (i++ < size){
         if (!put_user (((uint8_t *) buff + i), (uint8_t) input_getc ())) KILL();
         }
            return i;
        }

    lock_acquire(&file_lock);
    fm = get_fdmap(fd);
    if(fm){
        ret = file_read(fm->f, buff, size);
    }
    lock_release(&file_lock);
    
    if(ret == -1) KILL();
    return ret;
            
}

static void
sys_seek(int fd, unsigned pos){
    lock_acquire(&file_lock);
    struct fd_map *fm = get_fdmap(fd);
    if(fm){
        file_seek(fm->f, pos);
    }
    lock_release(&file_lock);

}

static unsigned
sys_tell(int fd){
    unsigned pos = -1;
    lock_acquire(&file_lock);
    struct fd_map* fm = get_fdmap(fd);
    if(fm){
        pos = file_tell(fm->f);
    }
    lock_release(&file_lock);
    return pos;
}

static void 
sys_close(int fd){
    
    struct fd_map* fm;
    lock_acquire(&file_lock);
    fm = get_fdmap(fd);
    if(fm){
    file_close(fm->f);
    list_remove(&fm->elem);
    free(fm);
    }
    lock_release(&file_lock);

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
sys_halt(void){
    shutdown_power_off();
}

static int sys_exec(const char* cmd_line){
    if(!check_string((char*)cmd_line)) KILL();
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
          break;
    case SYS_WAIT:
          f->eax = sys_wait((tid_t)checked_get(args,1));
          break;
    case SYS_CREATE:
          f->eax = sys_create((char*)checked_get(args,1), (unsigned) checked_get(args,2));
          break;
    case SYS_REMOVE:
          f->eax = sys_remove((char*)checked_get(args,1));
          break;
    case SYS_OPEN:
          f->eax = sys_open((char*)checked_get(args,1));
          break;
    case SYS_FILESIZE:
          f->eax = sys_filesize((int) checked_get(args,1));
          break;
    case SYS_WRITE:
          f->eax = sys_write((int)checked_get(args,1), (void*)checked_get(args,2),
                (unsigned)checked_get(args,3));  
          break;
    case SYS_READ:
        f->eax = sys_read( (int)checked_get(args,1),(void*)checked_get(args,2), (int)checked_get(args,3) );
         break; 
    case SYS_SEEK:
         sys_seek((int)checked_get(args,1),(unsigned)checked_get(args,2));
         break;
    case SYS_TELL:
         f-> eax = sys_tell((int)checked_get(args,1));
         break;
    case SYS_CLOSE:
         sys_close((int) checked_get(args,1));
         break;
    default:
          printf("Calling system call: number %d\n", checked_get(args,0));
        
        
  }
  /*
         *f->eax = args[1];
         *printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
         *thread_exit();
         */
}
