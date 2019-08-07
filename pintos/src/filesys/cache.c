#include "filesys/cache.h"
#define CACHE_SIZE 64
static struct ca_entry cache_table [CACHE_SIZE] = {{0,0,{0}}};
