Design Document for Project 3: File System
==========================================
## Cache

- NEW FILE filesys/cache.c

```
struct ca_entry{
    bool use;
    bool dirty;
    uint8_t data[BLOCK_SECTOR_SIZE];
};

uint8_t* cache_get(block_sector_t s, void* buffer);
uint8_t* cache_write (block_sector_t s, off_t off, off_t size, void* buffer);
int look_up(block_sector_t s);
int  pf_handler(block_sector_t s);
```
- Modify `inode_read_at() and inode_write_at()` using cache methods
- Modify `filesys_done()` to syn cache on system exit

## Indexed and Extensible file

