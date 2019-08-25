#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include <user/syscall.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "filesys/free-map.h"
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
    
    s = filesys_create(name, initial_size);

    return s;
}

static bool 
sys_remove(char* name){
    if(!check_string(name)) KILL();
    bool s;
    
    s = filesys_remove(name);

    return s;
}

static struct fd_map*
get_fdmap(int fd){
    lock_acquire(&file_lock);
    struct list_elem *e;
    struct list* l = &(thread_current()->fd_map_list);
     
    for(e = list_begin(l);e != list_end(l); e = list_next(e)){
        struct fd_map* fm = list_entry(e, struct fd_map, elem);
        if(fm->fd == fd){
            lock_release(&file_lock);
            return fm;
        }
    }

    lock_release(&file_lock);
    return NULL;

}
static int 
sys_open(char* file){
    if(!check_string(file)) KILL();
    int fd = -1;
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

    return fd;
}

static int 
sys_filesize(int fd){
    int size = -1;
    struct fd_map* fm = get_fdmap(fd);
    lock_acquire(&file_lock);
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

    int ret = -1;
    struct fd_map* fm = get_fdmap(fd);
    if(fm && !file_is_dir(fm->f)){
        ret = file_write(fm->f, buff, size);
    }
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

    fm = get_fdmap(fd);
    if(fm && !file_is_dir(fm->f)){
        ret = file_read(fm->f, buff, size);
    }
    
    if(ret == -1) KILL();
    return ret;
            
}

static void
sys_seek(int fd, unsigned pos){
    struct fd_map *fm = get_fdmap(fd);
    lock_acquire(&file_lock);
    if(fm){
        file_seek(fm->f, pos);
    }
    lock_release(&file_lock);

}

static unsigned
sys_tell(int fd){
    unsigned pos = -1;
    struct fd_map* fm = get_fdmap(fd);
    lock_acquire(&file_lock);
    if(fm){
        pos = file_tell(fm->f);
    }
    lock_release(&file_lock);
    return pos;
}

static void 
sys_close(int fd){
    
    struct fd_map* fm;
    fm = get_fdmap(fd);
    if(fm){
    file_close(fm->f);
    lock_acquire(&file_lock);
    list_remove(&fm->elem);
    lock_release(&file_lock);
    free(fm);
    }

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

static bool
sys_chdir(const char* dir){
    if(!check_string((char*)dir)) KILL();
    char dir_name[NAME_MAX+1];
    struct inode* in;
    struct dir* d;
    block_sector_t bs = dir_parse_path((char*)dir,dir_name);
    if(*dir_name=='\0'){
        //ROOT DIR
        thread_current()->wd = bs;
        return true;
    }
    
    if(!strcmp("..", dir_name)){
        in = inode_open(bs);
        if(in == NULL) return false;
        bs = inode_get_parent(in);
        inode_close(in);
    }else if(strcmp(".", dir_name)){
        d = dir_open(inode_open(bs));
        if(d == NULL) return false;
        if(!dir_lookup(d,dir_name,&in)){
            dir_close(d);
            return false;
        }
        bs = inode_get_inumber(in);
        inode_close(in);
        dir_close(d);
    }else{
        //d = dir_open(inode_open(bs));
        //if(d == NULL) return false;
        //bs = inode_get_parent(dir_get_inode(d));
        //dir_close(d);
    }
    
    thread_current()->wd = bs;

    return true;
}

static bool
sys_mkdir(const char* dir){
  block_sector_t inode_sector = 0;
  char file_name[NAME_MAX+1];
  block_sector_t path_sector  = dir_parse_path((char*)dir,file_name);  
  struct dir *d = dir_open(inode_open(path_sector));
  
  if(!strcmp("..",file_name) || !strcmp(".", file_name)){
      return false;
  }
  bool success = (dir != NULL
                    //find a sector to palce inde
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, 0, path_sector)
                  && dir_add (d, file_name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (d);

  return success;

}

static int 
sys_isdir(int fd){
    struct fd_map *fm = get_fdmap(fd);
    return file_is_dir(fm->f);
}

static int 
sys_inumber(int fd){
    struct fd_map *fm = get_fdmap(fd);
    return file_get_inumber(fm->f);
}

static bool
sys_readdir(int fd, char* name){
    if(!check_buff(name,READDIR_MAX_LEN + 1)) KILL();
    struct fd_map *fm = get_fdmap(fd);
    if(!fm) return false;
    return file_readdir(fm->f, name);

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
    case SYS_CHDIR:
         /* Change the current directory. */
         f->eax = sys_chdir((char*)checked_get(args,1));
         break;
    case SYS_MKDIR:
         /* Create a directory. */
         f->eax = sys_mkdir((char*)checked_get(args,1));
         break;
    case SYS_READDIR:
         /* Reads a directory entry. */
         f->eax = sys_readdir((int) checked_get(args,1),(char*) checked_get(args,2));
         break;
    case SYS_ISDIR:
         /* Tests if a fd represents a directory. */
         f->eax = sys_isdir((int)checked_get(args,1));
         break;
    case SYS_INUMBER:
         /* Returns the inode number for a fd. */
         f->eax = sys_inumber((int)checked_get(args,1));
         break;
    default:
          printf("Calling system call: number %d\n", checked_get(args,0));
        
        
  }
  /*
         *f->eax = args[1];
         *printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
         *thread_exit();
//         */
}
