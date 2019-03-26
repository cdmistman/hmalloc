#include <mmap.h>

#include "hmalloc.h"

// ===== DEFINES ===== //
#define TRUE 1
#define FALSE 0
#define PAGE_SZ 4096
#define ARENA_32 0
#define ARENA_64 1
#define ARENA_128 2
#define ARENA_256 3
#define ARENA_512 4
#define ARENA_1024 5
#define NUM_ARENAS 6

#define ARENA_SIZE 4

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0X20
#endif

// ===== MACROS ===== //
#define PAGE(page_size) mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)

// ===== TYPE DEFS ===== //
typedef unsigned long pointer_t;
typedef unsigned long count_t;
typedef unsigned long index_t;
typedef struct free_block 
{
    struct free_block_t* next;
} free_block_t;
typedef struct bin_t 
{
    size_t sz;
    size_t mem_allocd;
    index_t arena_index;
    pointer_t next_seg;
    free_block_t* free_list;
    struct bin_t* prev;
    struct bin_t* next;
} bin_t;
typedef struct arena_t
{
    bin_t* first;
    pthread_mutex_t mutex;
} arena_t;
typedef struct arena_list_t
{
    arena_t arenas[ARENA_SIZE];
} arena_list_t;

// ===== PRIVATE VARS ===== //
static hm_stats stats;
static arena_list_t arenas_list[NUM_ARENAS];
for (int ii = 0; ii < NUM_ARENAS; ++ii)
{
    for (int jj = 0; jj < ARENA_SIZE; ++jj)
    {
        arenas_list[ii]->arenas[jj]->mutex = PTHREAD_MUTEX_INITIALIZER;
        bin_t bin = mmap(0, PAGE_SZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        bin->sz = 2 ^ (5 + ii);
        bin->arena_index = jj;
        bin->free_list = NULL;
        bin->prev = NULL;
        bin->next = NULL;

        while (bin->mem_allocd < sizeof(bin_t)) 
            bin->mem_allocd += bin->sz;
        bin->next_seg = (pointer_t*)bin + mem_allocd;
    }
}

// ===== PRIVATE FUN DECS ===== //

// ===== PUBLIC FUN IMPS ===== //
void*
hmalloc(size_t size)
{
    if (size <= 0) return NULL;

    index_t arenas_index;
    if (size <= 32) arenas_index = ARENA_32;
    else if (size <= 64) arenas_index = ARENA_64;
    else if (size <= 128) arenas_index = ARENA_128;
    else if (size <= 256) arenas_index = ARENA_256;
    else if (size <= 512) arenas_index = ARENA_512;
    else if (size <= 1024) arenas_index = ARENA_1024;
    else arenas_index = -1;

    bin_t* bin;
    arena_t* arena;
    if (arenas_index >= 0)
    {
        index_t arena_index = 0;
        while (pthread_mutex_trylock(arenas_list[arenas_index]->arenas[arena_index]) != 0)
        {
            ++arena_index;
            if (arena_index >= NUM_ARENAS) arena_index = 0;
        }
        arena = &(arenas_list[arenas_index]->arenas[arena_index]);
        bin = arena->first;
    } 
    else 
    {
        bin = mmap(0, PAGE_SZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
}