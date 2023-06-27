SO - Tema 2 - Memory Allocator - 13.04.2023
Blotiu Mihnea-Andrei - 333CA

The main objective of this homework was to implement a memory allocator based on mmap and sbrk
in order to obtain the well known malloc, calloc, free and realloc.

That being said, the main points to implement malloc and calloc ar the same as the following with
the difference that in calloc we also set the memory to 0 after alloc:
    -  We firstly check if the size that we want to alloc is larger than the given threshold
    value. If it is we just alloc the needed size with mmap, add the new block to the list and
    return the new address;
    - If it is not the first case it means that it has to be put on the heap with sbrk. If it is 
    the first heap alloc we do a heap preallocation in order to alloc more than the needed space
    in order to use it in the future and for less future sbrks calls.
    - If it is not the first heap alloc it means that there is a chance to find an already free
    block on the heap. We try to find one firstly by merging and then splitting the already
    existing blocks on the heap. If not possible, we try to expand the last block on the heap if
    it was free. 
    - If we didn't find any free block on the heap that we can use we need to alloc additional
    memory to sbrk, a new block which we add to the list and return the new address.

For the free function the steps are pretty straight-forward:
    - If we try to free an NULL address we just won't do anything
    - Otherwise we will try to take the meta block from the given address
    - If the current meta block was a block that was allocated on the heap, we will just mark it
    as a free block in the list because we would like to reuse it in the future
    - Otherwise it means that the block we are trying to free was a block allocated with mmap which
    we won't be reusing so:
        * Firstly we delete the block from the list of memory;
        * We call munmap on the previously mmaped address;

For the realloc function I tried to use malloc and free already implemented functions as much as
possible as following:
    - If we try to realloc a previously NULL address it means that it has never been allocated
    before so we would like to malloc it for the first time;
    - If we try to realloc a previously valid address to a new size of 0 it means that we actually
    want to free that block so we just call free on that address
    - Otherwise we try to take the meta block from the current address. If that block was already
    freed before it means that it does not contain usefull information so we won't realloc it;
    - If the block was not freed before it means that now it is a valid realloc and we have the
    following cases:
        * If the new size of the realloc is bigger than the threshold value it means that we have
        to alloc it again and free the previous one as the block has to be allocated with mmap and
        those kind of blocks are not reused.
        * If the initial block that we want to realloc was initially allocated with mmap we are in
        the same situation as above because mmap blocks are not reused;
        * Otherwise it means that the new size is smaller than the threshold value so we are
        working just with addresses on the heap. Here we have three cases:
            + If the new size is smaller than the previous one we just have to split the initial
            block into two blocks and mark the second block as being free in order to be reused
            by other allocs;
            + If the new size is the same as the previous one we have nothing to do;
            + If the size is bigger than the previous one we will first try to expand the current
            block if that is possible. If it is not we will have to alloc a new block in with the
            given size and free the old one.

Problems durring the homework:
    * One important aspect in the whole homework is that I tried to keep a list of blocks somehow
    ordered. Always have the heap blocks before the mmaped blocks.
    * I did this because otherwise it was a pretty big problem when trying to expand addiacent free
    blocks. For example, there could have been one situation where two heap adiacent free blocks could
    have had between them one or more mmap blocks, and the actual algorithm would have not merged the
    two free blocks because it would not consider them as being adiacent.
    * Unfortunatelly I found out about this after a lot of hours of checking why the last test was
    failing cosindering that all the others would have passed event with the old implementation. 