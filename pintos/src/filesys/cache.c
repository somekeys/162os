#include "filesys/cache.h"
#include "lib/string.h"
#include "filesys/filesys.h"
#define CACHE_SIZE 64
static struct ca_entry cache_table [CACHE_SIZE] = {{0,0,-1,{0}}};
static int  pf_handler(block_sector_t s);

/* Search in the cache table for the sector C
 * and return the cache entry index
 * call pf_handler on cache miss*/
static int inline look_up(block_sector_t s){
    int i;
    for(i=0;i<CACHE_SIZE;i++){
       if(cache_table[i].id == s){
           break;
       }
    }
   
    i = i == CACHE_SIZE? pf_handler(s):i ; 
    return i;
}

/* Find given block S in the cache and copy it into the BUFFER
 * Read the block from disk to cache ,if the block is not in 
 * the cache
 */
void cache_get(block_sector_t s, void* buffer_){
   int i = look_up(s);
   uint8_t *buffer = buffer_;
    //find the cache index of the given sector
   
   
   //generate a page fault if given sector not found
   memcpy(buffer, cache_table[i].data, BLOCK_SECTOR_SIZE);
   cache_table[i].use =true;

}


/* Write SIZE byte from buffer to sector S at offset OFF in the cache
 * readin the sector if the sector is not inside the cache.
 * and the dirty filed in cache entry containg the sector is set to true
 */
off_t  cache_write (block_sector_t s, off_t off, off_t size, void* buffer_){
    int i = look_up(s);
    uint8_t* buffer = buffer_;

    memcpy(cache_table[i].data + off, buffer,size);
    cache_table[i].dirty = true;
    cache_table[i].use = true;

    return size;

}


/* Store a static pointer the iliterate the cahce table recursively
 * if the pointer point to a cache entry with use bit set to 0:
 *  1. if dirty bit bit in the entry is set write it to the disk
 *  2. read the sector S from disk to the entry 
 *  3. set use to true and dirty to false
 *  4. increment pointer
 *  5. return the entry index % BLOCK_SECTOR_SIZE
 * if the use bit set to 1:
 *  1.set the use bit to 0 
 *  2.increment the pointer % BLOCK_SECTOR_SIZE */

static int  pf_handler(block_sector_t s){
    static int ptr = 0;
    while(cache_table[ptr].use){
        cache_table[ptr].use = false;
        ptr++;
        ptr = ptr % CACHE_SIZE;
    }
    struct ca_entry cn = cache_table[ptr];
    
    if(cn.dirty){
        block_write_(fs_device,cn.id, cn.data);
    }
    
    block_read_ (fs_device,s, cn.data);
    cn.use = true;
    cn.dirty= false;
    cn.id = s;
    
    return ptr;
}

void cache_sync(void){
    struct ca_entry c_entry;
    int i;
    
    for(i = 0;  i<CACHE_SIZE; i++){
        c_entry = cache_table[i];
        
        if(c_entry.dirty){
            block_write_(fs_device,c_entry.id, c_entry.data);
            c_entry.dirty =false;
        }
    }
}

