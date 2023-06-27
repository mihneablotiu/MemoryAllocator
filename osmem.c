// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "helpers.h"

// The list of blocks that malloc, calloc and realloc share
struct block_meta *head;

// A parameter that tell us if it is the first alloc on the heap or not
int firstBrkAlloc;

// The alligned size of the block_meta structure
size_t blkMetaSize = ALIGN(sizeof(struct block_meta));

void *mmapAlloc(size_t blkSize)
{
	// We allocate the memory with mmap
	void *returnAddress = mmap(NULL, blkSize + blkMetaSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	// We check if the mmap did not fail
	DIE(returnAddress == ((void *) -1), "Error at mmap in malloc/calloc\n");
	struct block_meta *newBlock = (struct block_meta *) returnAddress;

	// We configure and add in the list the new allocated block
	configureMeta(newBlock, blkSize, STATUS_MAPPED);
	addElementInList(&head, newBlock, STATUS_MAPPED);

	// We return the new allocated memory address from the block
	return getAddressFromABlock(newBlock, blkMetaSize);
}

void *firstHeapAlloc(size_t thresholdValue)
{
	// We mark that we did the first alloc on heap so we won't do it again
	firstBrkAlloc = 1;

	// We do the heap preallocation with the thresholdValue size
	void *returnAddress = sbrk(thresholdValue);

	// We check if the sbrk failed or not
	DIE(returnAddress == ((void *) -1), "Error at sbrk in first heap alloc\n");

	struct block_meta *newBlock = (struct block_meta *) returnAddress;

	// If it did not fail, we configure and add in the list the new allocated block
	configureMeta(newBlock, thresholdValue - blkMetaSize, STATUS_ALLOC);
	addElementInList(&head, newBlock, STATUS_ALLOC);

	// We return the new allocated memory address from the block
	return getAddressFromABlock(newBlock, blkMetaSize);
}

struct block_meta *otherHeapAlloc(size_t blkSize)
{
	// We alloc exactly the size that we need on the heap
	void *returnAddress = sbrk(blkSize + blkMetaSize);

	// We check if the sbrk failed or not
	DIE(returnAddress == ((void *) -1), "Error at sbrk in first heap alloc\n");

	struct block_meta *newBlock = (struct block_meta *) returnAddress;

	// If it did not fail, we configure and add in the list the new allocated block
	configureMeta(newBlock, blkSize, STATUS_ALLOC);
	addElementInList(&head, newBlock, STATUS_ALLOC);

	// We return the new allocated memory block
	return newBlock;
}

void *os_malloc_calloc_helper(size_t blkSize, size_t thresholdValue, int isCalloc)
{
	// If the block size that we want to allocate is 0 then we don't do anything
	if (blkSize == 0)
		return NULL;

	// If the size is bigger than the threshold value we allocate with mmap
	if (blkSize + blkMetaSize >= thresholdValue) {
		void *returnValue = mmapAlloc(blkSize);

		// If it is a calloc, we also set the returned memory to 0
		if (isCalloc)
			memset(returnValue, 0, blkSize);

		return returnValue;
	}

	// If the size is lower than the threshold value and it is the
	// first alloc on the heap, we do a heap prealocation with sbrk
	if (blkSize + blkMetaSize < thresholdValue && firstBrkAlloc == 0) {
		void *returnValue = firstHeapAlloc(THRESHOLD);

		// If it is a calloc, we also set the returned memory to 0
		if (isCalloc)
			memset(returnValue, 0, blkSize);

		return returnValue;
	}

	// If it is not the first two cases above it means that we might have some free memory
	// in our list for the new block so we search for one free block
	struct block_meta *newBlock = findBlockWithSpecificSize(head, blkSize, blkMetaSize);

	// If there is no free block big enough for the new size we allocate one
	if (newBlock == NULL)
		newBlock = otherHeapAlloc(blkSize);

	void *returnValue = getAddressFromABlock(newBlock, blkMetaSize);

	// If it is a calloc, we also set the returned memory to 0
	if (isCalloc)
		memset(returnValue, 0, blkSize);

	// We return the new allocated memory address from the block
	return returnValue;
}

void *os_malloc(size_t size)
{
	int isCalloc = 0;

	return os_malloc_calloc_helper(ALIGN(size), THRESHOLD, isCalloc);
}

void os_free(void *ptr)
{
	// If we want to free a NULL pointer we don't do anything
	if (ptr == NULL)
		return;

	// We get the block from the given address
	struct block_meta *current = getBlockFromAddress(ptr, blkMetaSize);

	// If that block was previously on the heap we just mark it as free on the list
	// because we might use it again in the future. Otherwise we delete the block from the
	// list and we call munmap as the memory allocated with mmap is never reused
	if (current->status == STATUS_ALLOC) {
		current->status = STATUS_FREE;
	} else if (current->status == STATUS_MAPPED) {
		deleteElementFromList(&head, current);

		int returnValue = munmap((void *) current, current->size + blkMetaSize);

		DIE(returnValue == -1, "Error at munmap in free\n");
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	int isCalloc = 1;

	return os_malloc_calloc_helper(ALIGN(nmemb * size), getpagesize(), isCalloc);
}

void *os_realloc(void *ptr, size_t size)
{
	// If we want to realloc a NULL address it means that we have to malloc it for the first time
	if (ptr == NULL)
		return os_malloc(size);

	// If we want to realloc an address to a 0 size it means that we actually want to free the block
	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	size_t blkSize = ALIGN(size);
	struct block_meta *initialBlock = getBlockFromAddress(ptr, blkMetaSize);

	// If the block was initially free it means that it has no useful data so we cannot realloc it
	if (initialBlock->status == STATUS_FREE)
		return NULL;

	// If the new size to which we want to reallocate a block is bigger than the threshold or the
	// block that we want to reallocate was initially allocated with mmap we just call malloc for the
	// new size, copy the old data from the prevoius data and free the old address because we don't
	// reuse addresses that were allocated with mmap
	if (blkSize + blkMetaSize >= THRESHOLD || initialBlock->status == STATUS_MAPPED) {
		void *returnValue = os_malloc(blkSize);

		memcpy(returnValue, ptr, minimumValue(initialBlock->size, blkSize));
		os_free(ptr);

		return returnValue;
	}

	struct block_meta *newBlock = NULL;

	// If it does not fit on the previous case it means that the block was previously allocated
	// on the heap and the new size does not exceed the THRESHOLD value.

	// -> If the new size is lower than the previous size we just have to split the initial block
	//    into two blocks: one with the needed size that will still be used after the realloc,
	//    and a new block with the remaining size that is going to be marked as free after the
	//    realloc

	// -> If the new size is higher than the previous one, firstly we try to expand on the heap
	//    the block we have right now. If we cannot do that we have to malloc a new address for it
	//    and copy all the previous data to the new address.

	// -> If the new size is the same as the previous one, we don't do anything for realloc
	if (blkSize < initialBlock->size) {
		newBlock = splitBlock(initialBlock, blkSize, blkMetaSize);
		return getAddressFromABlock(newBlock, blkMetaSize);
	} else if (blkSize > initialBlock->size) {
		newBlock = expandBlockRealloc(head, initialBlock, blkSize, blkMetaSize);

		if (newBlock != NULL)
			return getAddressFromABlock(newBlock, blkMetaSize);

		void *returnValue = os_malloc(size);

		memcpy(returnValue, ptr, minimumValue(initialBlock->size, blkSize));
		os_free(ptr);

		return returnValue;
	} else {
		return ptr;
	}

	return NULL;
}
