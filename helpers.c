// SPDX-License-Identifier: BSD-3-Clause
#include "helpers.h"
#include "osmem.h"
void configureMeta(struct block_meta *newBlock, size_t size, int status)
{
	newBlock->size = size;
	newBlock->status = status;
	newBlock->next = NULL;
}

void addElementInList(struct block_meta **head, struct block_meta *current, int status)
{
	// If the list was initally NULL we just set the head as the current element
	if (*head == NULL) {
		*head = current;
		return;
	}

	if ((*head)->status == STATUS_MAPPED) {
		if (status == STATUS_MAPPED) {
			current->next = (*head)->next;
			(*head)->next = current;
		} else {
			current->next = (*head);
			(*head) = current;
		}

		return;
	}

	// We always add the mapped addresses to the end of the list and the heap addresses
	// after the last heap address already on the list but before the first mapped address
	struct block_meta *copy = *head;
	struct block_meta *copyNext = copy->next;

	if (status == STATUS_MAPPED) {
		while (copy->next != NULL)
			copy = copy->next;

		copy->next = current;
		current->next = NULL;

	} else if (status == STATUS_ALLOC) {
		while (copyNext != NULL) {
			if (copyNext->status == STATUS_MAPPED)
				break;

			copy = copyNext;
			copyNext = copy->next;
		}

		current->next = copyNext;
		copy->next = current;
	}
}

void deleteElementFromList(struct block_meta **head, struct block_meta *current)
{
	// If the list is null we have nothing to delete from it
	if (*head == NULL)
		return;

	// If the list has just one element, it means that it is the head so we delete the head
	if ((*head)->next == NULL) {
		*head = NULL;
		return;
	}

	struct block_meta *copy = *head;

	// Otherwise we search for the block in the list and if we find it we skip over it
	while (copy->next != NULL && copy->next != current)
		copy = copy->next;

	if (copy->next == NULL)
		return;

	copy->next = current->next;
}

struct block_meta *getLastBrkBlock(struct block_meta *head)
{
	struct block_meta *copy = head;

	// We iterate through the list until we find that the next block is a mmaped one so
	// we stop and return the last free or heap block
	while (copy->next != NULL) {
		if (copy->next->status == STATUS_MAPPED)
			break;

		copy = copy->next;
	}

	if (copy->status == STATUS_ALLOC || copy->status == STATUS_FREE)
		return copy;

	return NULL;
}

struct block_meta *splitBlock(struct block_meta *initial, size_t neededSize, size_t blkMetaSize)
{
	// We check if the initial size is bigger than the needed size plus the new size for a new
	// eventual block (which is made of a new head meta block and at lease 1 usable byte)
	// If it is we split the block in two marking the second block as being free and usable
	if (initial->size >= neededSize + ALIGN(1) + blkMetaSize) {
		struct block_meta *newBlock = (struct block_meta *) ((char *) initial + blkMetaSize + neededSize);

		newBlock->size = initial->size - neededSize - blkMetaSize;
		newBlock->status = STATUS_FREE;
		newBlock->next = initial->next;

		initial->size = neededSize;
		initial->status = STATUS_ALLOC;
		initial->next = newBlock;
	}

	// We return the initial block with the new size after split
	return initial;
}

void *getAddressFromABlock(struct block_meta *current, size_t blkMetaSize)
{
	return (void *) (((char *) current) + blkMetaSize);
}

struct block_meta *getBlockFromAddress(void *returnAddress, size_t blkMetaSize)
{
	return (struct block_meta *) (((char *) returnAddress) - blkMetaSize);
}

struct block_meta *
expandBlockRealloc(struct block_meta *head, struct block_meta *initial, size_t neededSize, size_t blkMetaSize)
{
	// If the list is empty we have nothing to do
	if (head == NULL)
		return NULL;

	struct block_meta *copy = head;
	struct block_meta *nextCopy = copy->next;

	// Otherwise we search for the block that we want to expand. If we find it and the next block
	// after is free it means we can expand this one. After each expansion we check to see if
	// the new block is big enough. If it is big enough it means that it is maybe too big
	// so we might split it before returning the new address
	while (nextCopy != NULL) {
		if (copy == initial) {
			if (nextCopy->status == STATUS_FREE) {
				copy->size += nextCopy->size + blkMetaSize;
				copy->next = nextCopy->next;
				nextCopy = nextCopy->next;

				if (copy->size >= neededSize)
					return splitBlock(copy, neededSize, blkMetaSize);
			} else {
				break;
			}
		} else {
			copy = nextCopy;
			nextCopy = nextCopy->next;
		}
	}

	// If we didn't return until now it means that we could not find a block big enough to fit
	// the new size. But if the block is the last one on the heap we can expand that one
	if (copy->next == NULL && copy == initial) {
		void *returnAddress = sbrk(neededSize - copy->size);

		DIE(returnAddress == ((void *) -1), "Error at sbrk in last block expand\n");
		configureMeta(copy, neededSize, STATUS_ALLOC);
		return copy;
	}

	return NULL;
}

struct block_meta *findBlockWithSpecificSize(struct block_meta *head, size_t neededSize, size_t blkMetaSize)
{
	// If the list is empty we have nothing to find
	if (head == NULL)
		return NULL;

	struct block_meta *copy = head;
	struct block_meta *nextCopy = copy->next;

	// Otherwise if there are two consecutive free blocks we will merge all of them
	while (nextCopy != NULL) {
		if (copy->status == STATUS_FREE && nextCopy->status == STATUS_FREE) {
			copy->size += nextCopy->size + blkMetaSize;
			copy->next = nextCopy->next;
			nextCopy = nextCopy->next;
		} else {
			copy = nextCopy;
			nextCopy = nextCopy->next;
		}
	}

	copy = head;
	struct block_meta *bestBlock = NULL;

	// After all the free blocks were merged we start searching for a best fit block for the
	// needed size (the one with the smallest size but larger than the needed one)
	while (copy != NULL) {
		if (copy->status == STATUS_FREE && copy->size >= neededSize) {
			if (bestBlock == NULL) {
				bestBlock = copy;
			} else {
				if (copy->size < bestBlock->size)
					bestBlock = copy;
			}
		}
		copy = copy->next;
	}

	// If we found such a block maybe its size is still bigger than the
	// needed one so we might need to split
	if (bestBlock != NULL) {
		bestBlock->status = STATUS_ALLOC;
		return splitBlock(bestBlock, neededSize, blkMetaSize);
	}

	// If we didn't find any block we can try to expand the last block on
	// the heap if it is free. Otherwise it means that we cannot find an already
	// existing block on the heap so we will return NULL
	copy = getLastBrkBlock(head);
	if (copy->status == STATUS_FREE) {
		void *returnAddress = sbrk(neededSize - copy->size);

		DIE(returnAddress == ((void *) -1), "Error at sbrk in last block expand\n");

		configureMeta(copy, neededSize, STATUS_ALLOC);

		return copy;
	}

	return NULL;
}

size_t minimumValue(size_t first, size_t second)
{
	if (first < second)
		return first;

	return second;
}
