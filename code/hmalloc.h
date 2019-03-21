#ifndef HMALLOC_H
#define HMALLOC_H

#include <stdlib.h>

typedef struct hm_stats
{
	long pages_mapped;
	long pages_unmapped;
	long chunks_allocated;
	long chunks_freed;
	long free_length;
} hm_stats;

void *hmalloc(size_t);
void hfree(void *);
void hprintstats();

#endif