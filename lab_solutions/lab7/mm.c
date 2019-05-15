/*
 * mm.c
 *
 * Brief intro:
 *   This allocator library uses implicit free list as the heap maintenance data structure. The header and footer of a block is used to store metadata for record in this allocation algorithm.
 *   Below is a simple diagram of the heap and blocks.
 *
       Allocated block:
  [header:<size>|<allocated>|<1>]__(align)
  [...........Payload...........]
  [...........Payload...........]
  [...........Payload...........]
  [...........Padding...........]
  [.............................]
  [footer:<size>|<allocated>|<1>]__(align)
 
       Free block:
  [header:<size>|<allocated>|<1>]__(align)
  [...........Randoms...........]
  [...........Randoms...........]
  [...........Randoms...........]
  [.............................]
  [footer:<size>|<allocated>|<1>]__(align)
 *  
 *
 * Optimization strategy:
 * 1. Use first-fit to better utilize CPU.
 * 2. Allocated bit reduced to 1 bit to increase memory able to be initiated.
 *
 * Author: Meng Yit Koh
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define PACK(size, allocated) (size | allocated)

#define READ(pointer) (*((int *)(pointer)))

#define WRITE(pointer, value) (*(int *)(pointer)) = value

#define GETSIZE(pointer) (READ(pointer) & ~0x1)

#define GETALLOCATED(pointer) (READ(pointer) & 0x1)

#define HEADER(pointer) ((void *)(pointer) - 4)

#define FOOTER(pointer) ((void *)(pointer) + GETSIZE(HEADER(pointer)) - 8)

#define PREVIOUS(pointer) ((void *)(pointer) - GETSIZE(((void *)(pointer) - 8))) // only used for coalesce(pointer)

#define NEXT(pointer) ((void *)(pointer) + GETSIZE(HEADER(pointer)))

static void *getSpace(size_t s);
static void *setHeaderFooter(void *pointer, size_t s); // only used for mm_malloc(size)
static void *coalesce(void *pointer);
static void *addMemory(size_t s);

static void * firstBlock; // heap header

// ilterate through memory and search for a space in memory of size s
static void * getSpace(size_t s)
{
	void *pointer;
	
	for (pointer = firstBlock; GETSIZE(HEADER(pointer)) > 0; pointer = NEXT(pointer)) {
		if ((s <= GETSIZE(HEADER(pointer))) && (!GETALLOCATED(HEADER(pointer)))) {
			return pointer;
		}
	}

	return NULL; // no suitable free memory block
}

// Set the Header and Footer of memory block. If condition allows, split the memory block
static void * setHeaderFooter(void *pointer, size_t s)
{
	size_t originalSize = GETSIZE(HEADER(pointer));

	if ((originalSize - s) >= 2 * ALIGNMENT) {
		WRITE(HEADER(pointer), PACK(s, 1));
		WRITE(FOOTER(pointer), PACK(s, 1));
		pointer = NEXT(pointer); // go to next block to split memory
		WRITE(HEADER(pointer), PACK((originalSize - s), 0));
		WRITE(FOOTER(pointer), PACK(originalSize - s, 0));
	} else {
		WRITE(HEADER(pointer), PACK(originalSize, 1));
		WRITE(FOOTER(pointer), PACK(originalSize, 1));
	}
}

static void * coalesce(void * pointer)
{
	size_t previousAllocated = GETALLOCATED(HEADER(PREVIOUS(pointer)));
	size_t nextAllocated = GETALLOCATED(HEADER(NEXT(pointer)));
	size_t size = GETSIZE(HEADER(pointer));

	if (previousAllocated && nextAllocated) {
		return pointer;
	} else if (!nextAllocated && previousAllocated) {
		size += GETSIZE(HEADER(NEXT(pointer)));
		WRITE(HEADER(pointer), PACK(size, 0));
		WRITE(FOOTER(pointer), PACK(size, 0));
	} else if (nextAllocated && !previousAllocated) {
		size += GETSIZE(HEADER(PREVIOUS(pointer)));
		WRITE(HEADER(PREVIOUS(pointer)), PACK(size, 0));
		WRITE(FOOTER(pointer), PACK(size, 0));
		pointer = PREVIOUS(pointer);
	} else {
		size += GETSIZE(HEADER(PREVIOUS(pointer))) + GETSIZE(FOOTER(NEXT(pointer)));
		WRITE(HEADER(PREVIOUS(pointer)), PACK(size, 0));
		WRITE(FOOTER(NEXT(pointer)), PACK(size, 0));
		pointer = PREVIOUS(pointer);
	}

	return pointer;
}

// if not enough memory, incease heap size by getting new memory from OS(using mem_sbrk(size))
static void * addMemory(size_t s)
{
	void * pointer;
	size_t size;

	if (s % 2 == 1) { // if odd
		size = 4 * (s + 1);
	} else { // if even
		size = s * 4;
	}

	if ((pointer = mem_sbrk(size)) == -1) {
		return NULL;
	}

	WRITE(HEADER(pointer), PACK(size, 0));
	WRITE(FOOTER(pointer), PACK(size, 0));
	WRITE(HEADER(NEXT(pointer)), PACK(0, 1)); // to prevent segmentation fault when program searches for free block beyond allocated heap

	return coalesce(pointer);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
        firstBlock = mem_sbrk(4*4);
        if (firstBlock == (void *)-1) {
                return -1;
        }

        WRITE(firstBlock, 0);
        WRITE(firstBlock + 4, PACK(8, 1));
        WRITE(firstBlock + 8, PACK(8, 1));
        WRITE(firstBlock + 12, PACK(0, 1));
        firstBlock += 8;
	
	return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    	// initialisation
    	size_t newSize = ALIGN(size + SIZE_T_SIZE);
	void * pointer;

	// if space exist
	pointer = getSpace(newSize);
	if (pointer != NULL) {
		setHeaderFooter(pointer, newSize);
		return pointer;
	}

	// if no such space, need to request from OS
	pointer = addMemory(newSize/4);
	if (pointer != NULL) { // assume always taken branch prediction
		setHeaderFooter(pointer, newSize);
		return pointer;
	}

	return NULL;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	size_t s = GETSIZE(HEADER(ptr));
	WRITE(HEADER(ptr), PACK(s, 0));
	WRITE(FOOTER(ptr), PACK(s, 0));

	coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void * oldptr, size_t size)
{
    void * newptr;
    size_t copySize;
    size_t newSize = ALIGN(size + SIZE_T_SIZE);
    size_t oldSize = GETSIZE(HEADER(oldptr));
    
    // if original size is enough, just return original pointer
    if (newSize <= oldSize) {
        return oldptr;
    }

    newptr = mm_malloc(size);
    if (newptr == NULL) {
      return NULL;
    }
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);

    return newptr;
}