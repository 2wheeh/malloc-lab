/*
 * mm-explicit.c - malloc package using explicit free blocks.
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
 * Free Block -> Doubly linked list
 * 
 *  31 . . .                                            . . . 2  1  0  bit
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+  <-- HDRP = bp - WSIZE
 * |    Header : Size of Block(including Header, Footer)    |  |  | F|  <-- Header
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+  <-- bp
 * |               Pointer to Predecessor in Linked list             |  <-- PRED
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+  <-- bp + WSIZE
 * |               Pointer to Successor in Linked list               |  <-- SUCC
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                                                                 |
 * .                                                                 .
 * .                                                                 .
 * .                         ... Garbage ...                         .
 * .                                                                 .
 * .                                                                 .
 * |                                                                 |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |    Footer : Size of Block(including Header, Footer)    |  |  | F|  <-- Footer
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * 
 * 
 ************************************************************************************/
 

/*************************************************************************************
 *--+--+--+--+--+--+--+--+--+--+--+--+--+--+--Heap Structure--+--+--+--+--+--+--+--+--+--+--+--+--+--+--*
 *
 *      Unused Prologue                 Regular              Regular                         Regular         Epilogue 
 *      block  block                    block 1              block 2                         block n         block hdr
 * Start +----+----+----+----+----+----+- - - - --+----+----+- - - - --+----+- - - - - +----+- - - - --+----+----+
 *   of  |    |16/1|PRED|SUCC|16/1| hdr|   ...    | ftr| hdr|   ...    | ftr|   ...    | hdr|   ...    | ftr| 0/1| 
 *  Heap +----+----+----+----+----+----+- - - - --+----+----+- - - - --+----+- - - - - +----+- - - - --+----+----+
 *       |         |         |         |          |         |          |                    |          |           -> Double word aligned
 *                 |                   |                    |
 *                 |                   |                    bp2 = next of bp1
 *                 |                   bp1 = prev of bp2        = bp1 + size of block 1 
 *                 |                       = bp2 - size of block1  
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
#define ALIGNMENT 8                 /* single word (4) or double word (8) alignment */

#define MAX(x, y) ((x) > (y) ? (x) : (y))    /* Bigger one between x and y */

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

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
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)))         /* Compute address of bp for Next Block */
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(HDRP(bp) - WSIZE)) /* Compute address of bp for Previous Block */

/* Given block ptr bp, compute address of entries of pointers to its predecessor and successor blocks */
#define PRED_PTR(bp)  ((char *)(bp)) 
#define SUCC_PTR(bp)  ((char *)(bp) + WSIZE)

/* Given block ptr bp, compute address of its predcessor and succesor in the linked list */
#define PRED_NODE(bp) (*(void **)(bp))
#define SUCC_NODE(bp) (*(void **)(SUCC_PTR(bp)))



/* heap checker customed from kang wook's */
#define UINT_CAST(p) ((size_t)p)
#ifdef DEBUG
# define CHECKHEAP() printf("\n==heap=checker=on==\n%s : %d\n", __func__,__LINE__); mm_checkheap(__LINE__);
#endif


/*********************************************************
 * Global variables
 ********************************************************/

static char *heap_listp;
static char *free_listp;

/*********************************************************
 * Function prototyes for helper funcs
 ********************************************************/

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void delete_node(void *bp);
void mm_checkheap(int lineno);

// static void insert_node(void *bp, size_t size);

/*********************************************************
 * mm_init - initialize the malloc package.
 ********************************************************/
int mm_init(void)
{   
    /* Create the initial empty heap 
     *                                                   Unused Prologue            Epilogue 
     *                                                   block  block               block hdr
     * Start +----+----+----+----+----+----+        Start +----+----+----+----+----+----+     
     *   of  |    |    |    |    |    |    |   ->     of  |    |16/1|    |    |16/1| 0/1|           
     *  Heap +----+----+----+----+----+----+         Heap +----+----+----+----+----+----+     
     *       |                                                      |                                           
     *       heap_listp                                             heap_listp                                   
     */
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1) return -1; 
    PUT(heap_listp, 0);                                   /* Alignment padding (Unused) */
    PUT(heap_listp + (1 * WSIZE), PACK(2*DSIZE, 1));      /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), NULL);                  /* PRED of Prologue */
    PUT(heap_listp + (3 * WSIZE), NULL);                  /* SUCC of Prologue */    
    PUT(heap_listp + (4 * WSIZE), PACK(2*DSIZE, 1));      /* Prologue footer */
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));            /* Eplilogue header */
    heap_listp += (DSIZE);
    free_listp = heap_listp;

    /* Extend the epmty heap with a free block of CHUNKSIZE bytes           PRED = NULL        PRED = heap_listp
     *                                                                      |                  |    SUCC = NULL    
     *       Unused Prologue            Epilogue             Unused Prologue|   SUCC = bp1     |    |                    Epilogue     
     *       block  block               block hdr            block  block   |   |              |    |                    block hdr         
     *  Start +----+----+----+----+----+----+           Start +----+----+----+----+----+----+----+----+ - - - +----+----+----+                   
     *    of  |    |16/1|    |    |16/1| 0/1|      ->     of  |    |16/1|PRED|SUCC|16/1|64/0|PRED|SUCC|  ...  |    |64/0| 0/1|   
     *   Heap +----+----+----+----+----+----+            Heap +----+----+----+----+----+----+----+----+ - - - +----+----+----+                              
     *                  |                                               |                   |                                        
     *                  heap_listp                                      heap_listp          bp1 = free_listp              
     */

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;

    return 0;
}


/*
 * delete_node - disconnect a free block from linked list. 
 */
static void delete_node(void *bp){
        if (free_listp == bp){                          /* when the block is happend to the top of free list, */   
            PUT(SUCC_PTR(PRED_NODE(bp)), NULL);         /* it means a successor ptr of it is pointing NULL. So we only have to care about predessecor of it */
            free_listp = PRED_NODE(bp);                 /* and must update free list top to bp */
        }
        else {                                          /* make predesseccor and successor point to each other */
            SUCC_NODE(PRED_NODE(bp)) = SUCC_NODE(bp);
            PRED_NODE(SUCC_NODE(bp)) = PRED_NODE(bp);
        }
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

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));           /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* New epilogue header */

    return coalesce(bp);
}



/*
 * coalesce - check prev block's ftr and next block's hdr.
 * when there is any other free block, coalesce them and move bp to more previous one.
 * update free_listp at Case 1 and (when next block is top of the linked list) at Case 2 and 4, 
 * which is to get closer to best-fit with first-fit in a sense of utiling space.
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  /* Last bit of prev block's footer */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  /* Last bit of next block's header */
    size_t size = GET_SIZE(HDRP(bp));                    /* size of current block */

    if (prev_alloc && next_alloc) {                      /* Case 1 : no free blocks aside */
        SUCC_NODE(free_listp) = bp;
        PRED_NODE(bp) = free_listp;
        PUT(SUCC_PTR(bp), NULL);
        free_listp = bp;
        return bp;
    }

    else if (prev_alloc && !next_alloc) {                   /* Case 2 : next block is free already */ 
        if (free_listp == NEXT_BLKP(bp)){                   /* when next block is happend to the top of free list, */   
            PRED_NODE(bp) = PRED_NODE(NEXT_BLKP(bp));       /* it means a successor ptr of it is pointing NULL. So we only have to care about predessecor of it */
            PUT(SUCC_PTR(bp), NULL);
            SUCC_NODE(PRED_NODE(NEXT_BLKP(bp))) = bp;       
            free_listp = bp;                                /* and must update free list top to bp */
        }
        else{
            PRED_NODE(bp) = PRED_NODE(NEXT_BLKP(bp));
            SUCC_NODE(bp) = SUCC_NODE(NEXT_BLKP(bp));

            SUCC_NODE(PRED_NODE(NEXT_BLKP(bp))) = bp;
            PRED_NODE(SUCC_NODE(NEXT_BLKP(bp))) = bp;
        }

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

    }

    else if (!prev_alloc && next_alloc) {                   /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                                   /* Case 4 */
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

/* 
 * find_fit - find a fitable free block which will get allocated.
 */
static void *find_fit(size_t asize)
{
    /* Fisrt-fit search 
     * search from the free_listp : LIFO
     */

    void *bp;

    for (bp = free_listp; GET_ALLOC(HDRP(bp)) != 1; bp = PRED_NODE(bp)){
        if (asize <= GET_SIZE(HDRP(bp))) return bp;
    }

    return NULL; /* No fit */
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // printf("%d\n", size);
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
 * place - place a block at allocated area and set leftover as free block
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    delete_node(bp);
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));          /* set hdr of allocted block */
        PUT(FTRP(bp), PACK(asize, 1));          /* set ftr of allocted block */
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));    /* set hdr of leftover block as free */
        PUT(FTRP(bp), PACK(csize-asize, 0));    /* set ftr of leftover block as free */
        PRED_NODE(bp) = free_listp;             /* initialize PRED of leftover free block */
        PUT(SUCC_PTR(bp), NULL);                /* initialize SUCC of leftover free block */
        SUCC_NODE(free_listp) = bp;
        free_listp = bp;                        /* the leftover be a new free list top */
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));          /* set hdr of allocted block */
        PUT(FTRP(bp), PACK(csize, 1));          /* set ftr of allocted block */
    }
}

/*
 * mm_free - Freeing a block and make it coalesced to contiguous blocks which are free already
 */
void mm_free(void *bp)
{   

    /* Ignore spurious reqs. */
    if (!bp) return ;

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);

    // #ifdef DEBUG
    //     CHECKHEAP(); 
    // #endif
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
  // If ptr is NULL, realloc is equivalent to mm_malloc(size)
  if (ptr == NULL)
    return mm_malloc(size);

  // If size is equal to zero, realloc is equivalent to mm_free(ptr)
  if (size == 0) {
    mm_free(ptr);
    return NULL;
  }
    
  /* Otherwise, we assume ptr is not NULL and was returned by an earlier malloc or realloc call.
   * Get the size of the current payload */
  size_t asize = MAX(ALIGN(size) + DSIZE, 2*DSIZE);
  size_t current_size = GET_SIZE(HDRP(ptr));

  void *bp;
  char *next = HDRP(NEXT_BLKP(ptr));
  size_t newsize = current_size + GET_SIZE(next);

  /* Case 1: Size is equal to the current payload size */
  if (asize == current_size)
    return ptr;

  // Case 2: Size is less than the current payload size 
  if ( asize <= current_size ) {

    if( asize > 2*DSIZE && (current_size - asize) > 2*DSIZE) {  

      PUT(HDRP(ptr), PACK(asize, 1));
      PUT(FTRP(ptr), PACK(asize, 1));
      bp = NEXT_BLKP(ptr);
      PUT(HDRP(bp), PACK(current_size - asize, 1));
      PUT(FTRP(bp), PACK(current_size - asize, 1));
      mm_free(bp);
      return ptr;
    }

    // allocate a new block of the requested size and release the current block
    bp = mm_malloc(asize);
    memcpy(bp, ptr, asize);
    mm_free(ptr);
    return bp;
  }

  // Case 3: Requested size is greater than the current payload size 
  else {

    // next block is unallocated and is large enough to complete the request
    // merge current block with next block up to the size needed and free the 
    // remaining block.
    if ( !GET_ALLOC(next) && newsize >= asize ) {

      // merge, split, and release
      delete_node(NEXT_BLKP(ptr));
      PUT(HDRP(ptr), PACK(asize, 1));
      PUT(FTRP(ptr), PACK(asize, 1));
      bp = NEXT_BLKP(ptr);
      PUT(HDRP(bp), PACK(newsize-asize, 1));
      PUT(FTRP(bp), PACK(newsize-asize, 1));
      mm_free(bp);
      return ptr;
    }  
    
    // otherwise allocate a new block of the requested size and release the current block
    bp = mm_malloc(asize); 
    memcpy(bp, ptr, current_size);
    mm_free(ptr);
    return bp;
  }

}


/*
 * mm_checkheap - check heap invariants for this implementation.
*/
void mm_checkheap(int lineno)
{
    char *heap_lo = mem_heap_lo();                      // pointing first word of the heap
    char *heap_hi = heap_lo + (mem_heapsize()-WSIZE);   // pointing last word of the heap
    char *bp;

    /* heap level check*/
    assert(GET(heap_lo) == 0);                                // check unused
    assert(GET(heap_lo + 1 * WSIZE) == PACK(2 * DSIZE, 1));   // check Prologue header
    // assert(GET(heap_lo + 2*WSIZE) == 0);                   // check Prologue PRED
    // assert(GET(heap_lo + 3*WSIZE) == 0);                   // check Prologue SUCC
    assert(GET(heap_lo + 4 * WSIZE) == PACK(2 * DSIZE, 1));   // check Prologue footer
    assert(GET(heap_hi) == PACK(0, 1));                       // check Epilogue block
    printf("heap level ok\n");
    /* block level */
    for(bp = heap_listp ; GET_SIZE(HDRP(bp)) > 0 ; bp = NEXT_BLKP(bp)) {
        assert(GET(HDRP(bp)) == GET(FTRP(bp)));                         // check header and footer match
        assert(!(UINT_CAST(bp) & 0x7));                                 // check if payload area aligned
        assert(GET_ALLOC(HDRP(bp)) | GET_ALLOC(HDRP(NEXT_BLKP(bp))));   // check contiguous free blocks
        assert(heap_lo < HDRP(bp) && FTRP(bp) < heap_hi);               // check heap bound

        // check all free blocks are in the free list
        if (!GET_ALLOC(HDRP(bp))) {
            void *pred_free;
            for (pred_free = PRED_NODE(free_listp); GET_ALLOC(HDRP(pred_free)) != 1; pred_free = PRED_NODE(pred_free)) {
                if (GET_ALLOC(HDRP(pred_free)) == 1)
                    break;
            }
            assert(pred_free);
        }
    } printf("block level ok\n");
    // detect cycle
    char * hare; char *tortoise;
    hare = tortoise = free_listp;
    printf("hare : %16p, tortoise: %16p\n", hare, tortoise);
    while(1) {
        if (GET_ALLOC(HDRP(hare)) == 1 || GET_ALLOC(HDRP(PRED_NODE(hare))) == 1)
            break;
        hare = PRED_NODE(PRED_NODE(hare));
        tortoise = PRED_NODE(tortoise);
        printf("hare : %16p, tortoise: %16p\n", hare, tortoise);
        assert(hare != tortoise);
    } printf("cycle check ok\n");

    printf("\n=======done=======\n");
}











