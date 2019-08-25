#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
  cache_sync();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size)
{
  block_sector_t inode_sector = 0;
  char file_name[NAME_MAX+1];
  block_sector_t path_sector  = dir_parse_path((char*)name,file_name);  

  if(path_sector== SECTOR_ERROR) return false;
  struct dir *dir = dir_open(inode_open(path_sector));
  if(!strcmp("..",file_name) || !strcmp(".", file_name)){
      return false;
  }
  bool success = (dir != NULL
                    //find a sector to palce inde
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct inode *inode = NULL;
  char file_name[NAME_MAX+1];
  if(*name=='\0'){
      return NULL;
  }
  block_sector_t path_sector  = dir_parse_path((char*)name,file_name);  
  if(path_sector== SECTOR_ERROR) return NULL;
  
  if(!path_sector){
      return NULL;
  }
  
  if(*file_name=='\0'){
      return file_open(inode_open(path_sector));
  }
  struct dir *dir = dir_open(inode_open(path_sector));
  
  if(!strcmp("..",file_name)){
      inode = inode_open(inode_get_parent(dir_get_inode(dir)));
      dir_close(dir);
      dir = NULL;
  }else if(!strcmp(".", file_name)){
      inode = inode_open(path_sector);
      dir_close(dir);
      dir = NULL;
  }

  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  char file_name[NAME_MAX+1];
  block_sector_t path_sector  = dir_parse_path((char*)name,file_name);  
  if(path_sector== SECTOR_ERROR) return false;
  struct dir *dir = dir_open(inode_open(path_sector));
  if(!strcmp("..",file_name)){
      return false;
  }else if(!strcmp(".", file_name)){
      return false;
  }
  
  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 0,ROOT_DIR_SECTOR))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
