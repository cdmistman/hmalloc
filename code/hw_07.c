#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>

#include "hw_07.h"

#define PAGE_SZ 4096
#define TRUE 1
#define FALSE 0

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

//--------------------------------------------------------------------------------------------------
// TYPE DEFINITIONS
//--------------------------------------------------------------------------------------------------
typedef unsigned long pointer;
typedef struct free_mem
{
	size_t size;
	struct free_mem *next;
} free_mem;

//--------------------------------------------------------------------------------------------------
// PRIVATE VARIABLES
//--------------------------------------------------------------------------------------------------
static free_mem *memory;
static hm_stats *stats;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//--------------------------------------------------------------------------------------------------
// PRIVATE FUNCTIONS
//--------------------------------------------------------------------------------------------------
void setup();
free_mem *add_page(size_t, free_mem *);
free_mem *allocate(free_mem *, free_mem *, size_t);
void check_if_munmap();
void check_combine();

void setup()
{
	if (memory == NULL)
	{
		memory = add_page(PAGE_SZ, NULL);

		stats = hmalloc(sizeof(hm_stats));
		stats->pages_mapped = 1;
		stats->pages_unmapped = 0;
		stats->chunks_allocated = 0;
		stats->chunks_freed = 0;
	}
}

free_mem *add_page(size_t size, free_mem *prev)
{
	while (size % PAGE_SZ != 0)
		++size;

	free_mem *res = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (prev != NULL)
	{
		pointer prev_ptr = (pointer)prev;
		pointer res_ptr = (pointer)res;
		if (prev_ptr + prev->size == res_ptr)
		{
			prev->size += res->size;
		}
		else
		{
			prev->next = res;
			res->size = size;
			res->next = NULL;
		}
	}

	if (stats != NULL)
		stats->pages_mapped += size / PAGE_SZ;

	return res;
}

free_mem *allocate(free_mem *before, free_mem *to_split, size_t tot_sz)
{
	if (tot_sz + sizeof(free_mem) > to_split->size)
	{
		if (before != NULL)
			before->next = to_split->next;
		return to_split;
	}

	pointer ptr = (pointer)to_split;
	ptr += to_split->size;
	ptr -= tot_sz;
	((free_mem *)ptr)->size = tot_sz;
	to_split->size -= tot_sz;

	return (free_mem *)ptr;
}

void check_if_munmap()
{
	size_t sz_acc = 0;
	free_mem *prev = NULL;
	free_mem *curr = memory;

	while (TRUE)
	{
		if (curr == NULL)
			break;

		sz_acc += curr->size;

		if (sz_acc > PAGE_SZ && prev != NULL && curr->size % PAGE_SZ == 0 && (pointer)curr % PAGE_SZ == 0)
		{
			free_mem *tmp = curr;
			prev->next = curr->next;
			curr = curr->next;
			munmap(tmp, tmp->size);
			if (stats != NULL)
				++(stats->pages_unmapped);
			continue;
		}

		prev = curr;
		curr = curr->next;
	}
}

void check_combine()
{
	free_mem *left = memory;
	free_mem *pre_r;
	free_mem *right;

	while (TRUE)
	{
		if (left == NULL)
			break;

		pre_r = left;
		right = left->next;
		while (TRUE)
		{
			if (right == NULL)
				break;

			pointer l_after = (pointer)left + left->size;
			pointer r_strt = (pointer)right;

			if (l_after == r_strt)
			{
				pre_r->next = right->next;
				left->size += right->size;
				right = right->next;
				continue;
			}

			pre_r = right;
			right = right->next;
		}

		left = left->next;
	}
}

//--------------------------------------------------------------------------------------------------
// PUBLIC FUNCTIONS
//--------------------------------------------------------------------------------------------------
void *hmalloc(size_t size)
{
	if (size <= 0)
		return NULL;

	pthread_mutex_lock(&mutex);
	setup();

	size_t tot_sz = size + sizeof(size_t);
	while (tot_sz % 8 != 0)
		++tot_sz;

	free_mem *prev = NULL;
	free_mem *curr = memory;
	free_mem *best_prev = NULL;
	free_mem *best = NULL;

	while (TRUE)
	{
		if (curr == NULL)
		{
			if (best == NULL)
			{
				best_prev = prev;
				best = add_page(tot_sz, prev);
			}
			break;
		}

		if (curr->size >= tot_sz && (best == NULL || curr->size < best->size))
		{
			best_prev = prev;
			best = curr;
		}

		prev = curr;
		curr = curr->next;
	}

	if (stats != NULL)
		++(stats->chunks_allocated);

	void *res = (void *)allocate(best_prev, best, tot_sz);
	pthread_mutex_unlock(&mutex);
	return res;
}

void hfree(void *to_free)
{
	if (to_free == NULL)
		return;

	pthread_mutex_lock(&mutex);
	pointer ptr = (pointer)to_free;
	ptr -= sizeof(size_t);

	free_mem *fred = (free_mem *)ptr;
	fred->next = NULL;

	free_mem *curr = memory;
	free_mem *prev = NULL;
	while (TRUE)
	{
		if (curr == NULL)
		{
			if (prev != NULL)
				prev->next = fred;
			break;
		}
		if (((pointer)curr + curr->size) == ptr)
		{
			curr->size += fred->size;
			break;
		}

		prev = curr;
		curr = curr->next;
	}

	if (stats != NULL)
		++(stats->chunks_freed);

	check_combine();
	check_if_munmap();
	pthread_mutex_unlock(&mutex);
}

void *hrealloc(void *to_realloc, size_t to_size)
{
	if (to_realloc == NULL || to_size == 0)
	{
		hfree(to_realloc);
		return NULL;
	}

	void *res = hmalloc(to_size);
	
	pointer old = (pointer)to_realloc;
	old -= sizeof(long);

	size_t sz = *((long *)old);
	memmove(res, to_realloc, min(sz, to_size));
	hfree(to_realloc);
	return res;
}

void hprintstats()
{
	pthread_mutex_lock(&mutex);
	if (stats == NULL)
	{
		printf("No stats found\n");
		pthread_mutex_unlock(&mutex);
		return;
	}

	stats->free_length = 0;

	free_mem *curr = memory;
	while (TRUE)
	{
		if (curr == NULL)
		{
			break;
		}
		stats->free_length += curr->size;
		curr = curr->next;
	}

	fprintf(stderr, "\n== husky malloc stats ==\n");
	fprintf(stderr, "Mapped:   %ld\n", stats->pages_mapped);
	fprintf(stderr, "Unmapped: %ld\n", stats->pages_unmapped);
	fprintf(stderr, "Allocs:   %ld\n", stats->chunks_allocated);
	fprintf(stderr, "Frees:    %ld\n", stats->chunks_freed);
	fprintf(stderr, "Freelen:  %ld\n", stats->free_length);
	pthread_mutex_unlock(&mutex);
}