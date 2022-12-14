/*
 * mm-implicit.c - malloc package using implicit free blocks.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team #3",
    /* First member's full name */
    "Wonhee Lee",
    /* First member's email address */
    "2wheeh@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/************************************************************************************
 *--+--+--+--+--+--+--+--+--+--+--Block Structure--+--+--+--+--+--+--+--+--+--+--*
 *
 * A = allocated
 * F = free
 * 
 * Allocated Block
 * 
 *  31 . . .                                            . . . 2  1  0  bit
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+  <-- HDRP = bp - WSIZE
 * |    Header : Size of Block (including Header, Footer)   |  |  | A|  <-- Header
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+  <-- bp
 * |                                                                 |
 * .                                                                 .
 * .                                                                 .
 * .                   ... Payload and padding ...                   .
 * .                                                                 .
 * .                                                                 .
 * |                                                                 |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+  <-- FTRP = bp + block size - DSIZE
 * |    Footer : Size of Block (including Header, Footer)   |  |  | A|  <-- Footer
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * 
 * 
 * Free Block
 * 
 *  31 . . .                                            . . . 2  1  0  bit
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |    Header : Size of Block(including Header, Footer)    |  |  | F|
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                                                                 |
 * .                                                                 .
 * .                                                                 .
 * .                         ... Garbage ...                         .
 * .                                                                 .
 * .                                                                 .
 * |                                                                 |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |    Footer : Size of Block(including Header, Footer)    |  |  | F|
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * 
 * 
 ************************************************************************************/
 

/*************************************************************************************
 *--+--+--+--+--+--+--+--+--+--+--+--+--+--+--Heap Structure--+--+--+--+--+--+--+--+--+--+--+--+--+--+--*
 *
 *      Unused Prologue       Regular              Regular                         Regular         Epilogue 
 *      block  block          block 1              block 2                         block n         block hdr
 * Start +----+----+----+----+- - - - --+----+----+- - - - --+----+- - - - - +----+- - - - --+----+----+
 *   of  |    | 8/1| 8/1| hdr|   ...    | ftr| hdr|   ...    | ftr|   ...    | hdr|   ...    | ftr| 0/1| 
 *  Heap +----+----+----+----+- - - - --+----+----+- - - - --+----+- - - - - +----+- - - - --+----+----+
 *       |         |         |          |         |          |                    |          |           -> Double word aligned
 *                 |         |                    |
 *                 |         |                    bp2 = next of bp1
 *                 |         bp1 = prev of bp2        = bp1 + size of block 1 
 *                 |             = bp2 - size of block1  
 *                 *heap_listp    
 * 
 * 
 ************************************************************************************/


/*********************************************************
 * Setting Constants and Macros
 ********************************************************/
/* Basic constants and macros */
#define WSIZE       4               /* Word and header/footer size (bytes)*/
#define DSIZE       8               /* Double word size (bytes) */
#define CHUNKSIZE   (1<<12)         /* Extend heap by 4kb (page size) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))    /* Bigger one between x and y */

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
/* p is mostly (void*), which should be casted to proper type */
#define GET(p)         (*(unsigned int *)(p))         /* Casting address p and read a word at p */
#define PUT(p, val)    (*(unsigned int *)(p) = (val)) /* Casting address p and write a word(val) at p */

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)    ((GET(p)) & ~0x7)     /* Read the block size data except last 3 bits */
#define GET_ALLOC(p)   ((GET(p)) & 0x1)      /* Read the last bit (A/F) */

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      /* Compute address of header : bp - word size */
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) /* Compute address of footer : bp + block size - double size */

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp))) /* Compute address of bp for Next Block */
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(HDRP(bp) - WSIZE)) /* Compute address of bp for Previous Block */


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))



/*********************************************************
 * Global variables
 ********************************************************/

static char *heap_listp;

/*********************************************************
 * Function prototyes for helper funcs
 ********************************************************/

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/*********************************************************
 * mm_init - initialize the malloc package.
 ********************************************************/
int mm_init(void)
{
    /* Create the initial empty heap 
     *                                                   Unused Prologue  Epilogue 
     *                                                   block  block     block hdr
     * Start +----+----+----+----+                  Start +----+----+----+----+     
     *   of  |    |    |    |    |        ->          of  |    | 8/1| 8/1| 0/1|           
     *  Heap +----+----+----+----+                   Heap +----+----+----+----+     
     *       |                                                      |                                 
     *       heap_listp                                             heap_listp                         
     */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) return -1; 
    PUT(heap_listp, 0);                             /* Alignment padding (Unused) */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        /* Eplilogue header */
    heap_listp += (2*WSIZE);

    /* Extend the epmty heap with a free block of CHUNKSIZE bytes
     * 
     *       Unused Prologue  Epilogue             Unused Prologue                              Epilogue     
     *       block  block     block hdr            block  block                                 block hdr         
     *  Start +----+----+----+----+           Start +----+----+----+----+----+ - - - +----+----+----+                   
     *    of  |    | 8/1| 8/1| 0/1|      ->     of  |    | 8/1| 8/1|64/0|    |  ...  |    |64/0| 0/1|                           
     *   Heap +----+----+----+----+            Heap +----+----+----+----+----+ - - - +----+----+----+                              
     *                  |                                     |                                                                       
     *                  heap_listp                            heap_listp                         
     */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    
    return 0;
}

/*
 * extend_heap - extend the heap size. place a free block which have size maintaing alignment at the end of heap.
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

    /* Initialzie free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));           /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * mm_free - Freeing a block and make it coalesced to contiguous blocks which are free already
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * coalesce - check prev block's ftr and next block's hdr.
 * when there is any other free block, coalesce them and move bp to more previous one.
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  /* Last bit of prev block's footer */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  /* Last bit of next block's header */
    size_t size = GET_SIZE(HDRP(bp));                    /* size of current block */

    if (prev_alloc && next_alloc) {                     /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) {                /* Case 2 */ 
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {                /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                               /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       /* Adjusted block size */
    size_t extendsize;  /* Amount to extend heap if no fit */
    char *bp;           

    /* Ignore spurious requests */
    if (size == 0) return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); /* round up to nearst 8's multiple */ 

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and plack the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    place(bp, asize);
    return bp;
}

/* 
 * find_fit - find a fitable free block which will get allocated.
 */
static void *find_fit(size_t asize)
{
    /* Fisrt-fit search*/
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) return bp;
    }

    return NULL; /* No fit */
}

/*
 * place - place a block at allocated area and set leftover as free block
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));          /* set hdr of allocted block */
        PUT(FTRP(bp), PACK(asize, 1));          /* set ftr of allocted block */
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));    /* set hdr of leftover block as free */
        PUT(FTRP(bp), PACK(csize-asize, 0));    /* set ftr of leftover block as free */
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));          /* set hdr of allocted block */
        PUT(FTRP(bp), PACK(csize, 1));          /* set ftr of allocted block */

    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr)); 
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














