/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define DIE(assertion, call_description)						\
	do {														\
		if (assertion) {										\
			fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);	\
			perror(call_description);							\
			exit(errno);										\
		}														\
	} while (0)

/* Structure to hold memory block metadata */
struct block_meta {
	size_t size;
	int status;
	struct block_meta *next;
};

/* Function that configures a give block with the specific size and status */
void configureMeta(struct block_meta *newBlock, size_t size, int status);

/* A function that adds a new block in the blocks list depending on its status */
void addElementInList(struct block_meta **head, struct block_meta *current, int status);

/* A function that gets the address from a meta block given as parameter */
void *getAddressFromABlock(struct block_meta *current, size_t blkMetaSize);

/* A function that gets the meta block from a give address */
struct block_meta *getBlockFromAddress(void *returnAddress, size_t blkMetaSize);

/* A function that searches for a block in the list and if it finds it then it is deleted*/
void deleteElementFromList(struct block_meta **head, struct block_meta *current);

/* A function that returns the last block allocated on the heap */
struct block_meta *getLastBrkBlock(struct block_meta *head);

/* A function that splits a initial block into two blocks depending on the needed size*/
struct block_meta *splitBlock(struct block_meta *initial, size_t neededSize, size_t blkMetaSize);

/* A function that merges all the free blocks on the heap and returns the best fit for a
new block that has to be allocated on the heap with the needed size. */
struct block_meta *findBlockWithSpecificSize(struct block_meta *head, size_t neededSize, size_t blkMetaSize);

/* A function that tries to expand a already allocated block on the heap in case of a realloc
to the needed size */
struct block_meta *expandBlockRealloc(struct block_meta *head, struct block_meta *initial,
									  size_t neededSize, size_t blkMetaSize);

/* A function that returns the minimum value between two values */
size_t minimumValue(size_t first, size_t second);

/* Block metadata status values */
#define STATUS_FREE   0
#define STATUS_ALLOC  1
#define STATUS_MAPPED 2
