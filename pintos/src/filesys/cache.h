#include <stdbool.h>
#include "filesys/inode.h"
#include "devices/block.h"

struct ca_entry{
    bool use;
    bool dirty;
    uint8_t data[BLOCK_SECTOR_SIZE];
};

/* Find given block S in the cache and copy it into the BUFFER
 * Read the block from disk to cache ,if the block is not in 
 * the cache
 */
uint8_t* cache_get(block_sector_t s, void* buffer);

/* Write SIZE byte from buffer to sector S at offset OFF in the cache
 * readin the sector if the sector is not inside the cache.
 * and the dirty filed in cache entry containg the sector is set to true
 */
uint8_t* cache_write (block_sector_t s, off_t off, off_t size, void* buffer);


//static
//static struct ca_entry cache_table [CACHE_SIZE] = {{0,0,{0}}};

/* Search in the cache table for the sector C
 * and return the cache entry index
 * call pf_handler on cache miss*/
int look_up(block_sector_t s);

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

int  pf_handler(block_sector_t s);
