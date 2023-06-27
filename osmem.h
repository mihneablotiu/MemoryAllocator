/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "printf.h"

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define THRESHOLD (128 * 1024)

/* The function that allocates memory with mmap if the block size given is
bigger than the THRESHOLD above. It configures the block's size and status
and then adds it to the list */
void *mmapAlloc(size_t blkSize);

/* The function that allocates memory with sbrk if the block size given is
below than the THRESHOLD above and it is the first alloc on the heap.
It does a heap preallocation, of the THRESHOLD size, configures the block's
size and status and then adds it to the list */
void *firstHeapAlloc(size_t thresholdValue);

/* The function that allocates memory with sbrk if the block size given is
below than the THRESHOLD above and it is not the first alloc on the heap.
It does an exact value sbrk, of the blkSize size given, configures the block's
size and status and then adds it to the list */
struct block_meta *otherHeapAlloc(size_t blkSize);

/* The function that manages all the allocs and diferences between malloc and
calloc by the isCalloc parameter. It chooses which function to call from the ones
above depending on the blkSize that needs to be alocated and returns the new
allocated memory address. If there is no need for allocating a new block it calls
the specific functions for searching a block in the already existing ones
from the list. */
void *os_malloc_calloc_helper(size_t blkSize, size_t thresholdValue, int isCalloc);

// The malloc standard function
void *os_malloc(size_t size);

// The free standard function
void os_free(void *ptr);

// The calloc standard function
void *os_calloc(size_t nmemb, size_t size);

// The realloc standard function
void *os_realloc(void *ptr, size_t size);
