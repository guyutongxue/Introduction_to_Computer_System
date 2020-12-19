/**
 * @file mm.c
 * @author Guyutongxue (1900012983@pku.edu.cn)
 * @brief Handin file of MallocLab
 * @version 0.2
 * @date 2020-12-18 ~ 12-19
 * Implementation of @c malloc , @c free , @c realloc and @c calloc .
 * Based on Segregated fit lists, with:
 * - *First Free* policy of placement
 * - LIFO free block ordering
 * - Bundary tag coalescing
 * @copyright Copyright (c) 2020 Guyutongxue
 *
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "./mm.h"
#include "./memlib.h"

#ifndef __GNUC__
#error This file must be compiled under GCC.
#endif

#if __STDC_VERSION__ < 199901L
#error You should compile this file with flag -std=gnu99.
#endif

// Header/footer printing macros
#define PACK_FMT "(%#x, %s, %s)"
#define PACK_ARG(p) \
  GET_SIZE(p), GET_BTAG(p) ? "ALLOC" : "FREE", GET_ALLOC(p) ? "ALLOC" : "FREE"

// DEBUG may be defined in Makefile
#ifdef DEBUG
#define dbg_printf(FORMAT, ...) \
  printf("%s(%d): " FORMAT "\n", __func__, __LINE__, ##__VA_ARGS__)

#define CHECK_HEAP() my_checkheap(__func__, __LINE__)
#else
#define dbg_printf(...)
#define CHECK_HEAP()
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

// Alignment set to double word.
#define ALIGNMENT 8

/// Rounds up to the nearest multiple of @c ALIGNMENT
#define ALIGN(p) (((size_t)(p) + ((ALIGNMENT)-1)) & ~0x7)

// Basic types and sizes
typedef uint32_t WORD;   ///< WORD is 32-bit
typedef uint64_t DWORD;  ///< DWORD (double WORD) is 64-bit
#define WORD_SIZE sizeof(WORD)
#define DWORD_SIZE sizeof(DWORD)

/* Explanation of a block:
 *
 * Allocated:
 * +--------+----------------------------------------+
 * | HEADER |         PAYLOAD (incl. align)          |
 * +--------+----------------------------------------+
 *     4B   ^
 *          |
 *          bp (base pointer)
 *
 * Free:
 * +--------+-----------+-----------+-------+--------+
 * | HEADER | NEXT_FREE | PREV_FREE |  ...  | FOOTER |
 * +--------+-----------+-----------+-------+--------+
 *     4B   ^     4B          4B                4B
 *          |
 *          bp (base pointer)
 *
 * MIN_PAYLOAD_SIZE == 12B
 * MIN_BLOCK_SIZE   == 16B
 * 
 * *Optimization*
 * NEXT_FREE & PREV_FREE records diff between pointer and heap_begin.
 *
 * HEADER & FOOTER:
 * +-------------------------------------+---+---+---+
 * |        SIZE (first 29 bits)         |   | B | A |
 * +-------------------------------------+---+---+---+
 *                  29b                    1b  1b  1b
 * A - Is allocated?
 * B - Boundary tag
 *
 */
#define MIN_PAYLOAD_SIZE 12   ///< Minimal payload alignment
#define MIN_BLOCK_SIZE 16     ///< Minimal block size
#define INIT_SIZE (1 << 6)    ///< Initial heap size
#define CHUNK_SIZE (1 << 12)  ///< Extend heap by this amount

/// Get the greater value of @c x and @c y
#define MAX(x, y) ((x) > (y) ? (x) : (y))
/// Get the less value of @c x and @c y
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/// Error return value of functions return pointer
#define ERRPTR ((void*)-1)

/// Get a @c WORD from pointer @c p
#define GET_WORD(p) (*(WORD*)(p))
/// Set @c value to the @c WORD located at @c p
#define PUT_WORD(p, val) (*(WORD*)(p) = (val))

/// Get @c size from a @c WORD
#define GET_SIZE(p) (GET_WORD(p) & ~0x7)
/// Get Boundary tag from a @c WORD
#define GET_BTAG(p) (GET_WORD(p) & 0x2)
/// Get @c alloc from a @c WORD
#define GET_ALLOC(p) (GET_WORD(p) & 0x1)

/// btag enums
#define BTAG_KEEP ((WORD)(-1))  ///< Keep btag as original
#define BTAG_ALLOC 0x2          ///< Set btag to Allocated
#define BTAG_FREE 0x0           ///< set btag to Freed

/// Put Pack( @c size , @c btag , @c alloc ) to @c WORD located at @c p
#define PUT_PACK(p, size, btag, alloc)                                 \
  ((btag) == BTAG_KEEP ? PUT_WORD((p), (size) | (alloc) | GET_BTAG(p)) \
                       : PUT_WORD((p), (size) | (alloc) | (btag)))
#define PUT_ALLOC_BTAG(p) (GET_WORD(p) |= 0x2)
#define PUT_FREE_BTAG(p) (GET_WORD(p) &= ~0x2)

/// Get the address of header/footer of a block
#define GET_HEADER(bp) ((void*)((char*)(bp)-WORD_SIZE))
#define GET_FOOTER(bp) \
  ((void*)((char*)(bp) + GET_SIZE(GET_HEADER(bp)) - DWORD_SIZE))

/// Get the @c bp of next/previous block
#define GET_NEXT_BLOCK(bp) ((void*)((char*)(bp) + GET_SIZE(GET_HEADER(bp))))
#define GET_PREV_BLOCK(bp) \
  ((void*)((char*)(bp)-GET_SIZE((char*)(bp)-DWORD_SIZE)))

/// Get the address of next/previous free block using seglist
#define GET_NEXT_FREE(bp)          \
  ({                               \
    WORD val = GET_WORD(bp);       \
    val ? val + heap_begin : NULL; \
  })
#define GET_PREV_FREE(bp)                         \
  ({                                              \
    WORD val = GET_WORD((char*)(bp) + WORD_SIZE); \
    val ? val + heap_begin : NULL;                \
  })
#define SET_NEXT_FREE(bp, val) \
  PUT_WORD((bp), (WORD)((val) ? ((char*)(val)-heap_begin) : 0))
#define SET_PREV_FREE(bp, val)      \
  PUT_WORD((char*)(bp) + WORD_SIZE, \
           (WORD)((val) ? ((char*)(val)-heap_begin) : 0))

/// Automatic choose appropriate seglist index
#define SEGLIST_AUTO ((size_t)-1)

/// How many seglist
#define SEGLIST_SIZE 17

/// The begin position of available heap ( @c bp of prologue)
static char* heap_begin = NULL;

/// Array of seglists
static void** seglist;

// Helper function declarations

static void* extend_heap(size_t);
static void* coalesce(void*);
static void* find_fit(size_t);
static void place(void*, size_t);

void my_checkheap(const char*, int);

static size_t seglist_get_index(size_t);
static void seglist_insert(void*, size_t);
static void seglist_remove(void*, size_t);
static void* seglist_find(size_t, size_t);

/**
 * @brief Initialize dynamic allocator
 *
 * @return -1 on error, 0 on success
 */
int mm_init(void) {
  if ((heap_begin = mem_sbrk(SEGLIST_SIZE * DWORD_SIZE + 4 * WORD_SIZE)) ==
      ERRPTR)
    return -1;
  seglist = (void**)heap_begin;
  for (int i = 0; i < SEGLIST_SIZE; i++) {
    seglist[i] = NULL;
  }
  heap_begin = (char*)(seglist + SEGLIST_SIZE);
  PUT_WORD(heap_begin, 0);  // Aligment padding
  // prologue header
  PUT_PACK(heap_begin + 1 * WORD_SIZE, DWORD_SIZE, BTAG_FREE, 1);
  // prologue footer
  PUT_PACK(heap_begin + 2 * WORD_SIZE, DWORD_SIZE, BTAG_FREE, 1);
  // epilogue header
  PUT_PACK(heap_begin + 3 * WORD_SIZE, 0, BTAG_ALLOC, 1);
  heap_begin += DWORD_SIZE;
  if (extend_heap(INIT_SIZE) == NULL) return -1;
  CHECK_HEAP();
  return 0;
}

/**
 * @brief Allocate @c size bytes of memory.
 *
 * @param size The size of allocated memory.
 * @return Pointer to allocated memory with ALIGNMENT
 */
void* malloc(size_t size) {
  size_t allocated_size;
  if (size == 0) return NULL;
  if (size <= MIN_PAYLOAD_SIZE)
    allocated_size = MIN_BLOCK_SIZE;
  else
    allocated_size = ALIGN(WORD_SIZE + size);
  void* bp;
  if ((bp = find_fit(allocated_size))) {
    place(bp, allocated_size);
    return bp;
  }
  size_t ext_size = MAX(allocated_size, CHUNK_SIZE);
  if ((bp = extend_heap(ext_size / WORD_SIZE))) {
    place(bp, allocated_size);
    return bp;
  }
  return NULL;
}

/**
 * @brief Free a block allocated by @c malloc , @c realloc or @c calloc
 *
 * @param ptr Pointer to the block will be freed
 */
void free(void* ptr) {
  if (!ptr) return;
  size_t size = GET_SIZE(GET_HEADER(ptr));
  PUT_PACK(GET_HEADER(ptr), size, BTAG_KEEP, 0);
  PUT_PACK(GET_FOOTER(ptr), size, BTAG_KEEP, 0);
  void* next_block = GET_NEXT_BLOCK(ptr);
  PUT_FREE_BTAG(GET_HEADER(next_block));
  coalesce(ptr);
}

/**
 * @brief Re-allocation
 * Re-allocate the previously allocated block in @c oldptr ,
 * making the new block @c size bytes long.
 * @param oldptr Pointer to previously allocated block
 * @param size New block size
 * @return Point to new allocated block
 */
void* realloc(void* oldptr, size_t size) {
  if (size == 0) {
    free(oldptr);
    return NULL;
  }
  if (oldptr == NULL) {
    return malloc(size);
  }
  void* newptr;
  if ((newptr = malloc(size)) == NULL) return NULL;
  memcpy(newptr, oldptr, MIN(size, GET_SIZE(GET_HEADER(oldptr))));
  free(oldptr);
  return newptr;
}

/**
 * @brief Allocation with initialization
 * Allocate @c nmemb elements of @c size bytes each, all initialized to 0.
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return Pointer to first element allocated
 */
void* calloc(size_t nmemb, size_t size) {
  size_t total_size = size * nmemb;
  void* bp = malloc(total_size);
  if (!bp) return NULL;
  memset(bp, 0, total_size);
  return bp;
}

void mm_checkheap(int lineno) {}  // Shut up linker complaint.

// Helper function definitions

/**
 * @brief Extend heap with free blocks
 *
 * @param words How many @c WORD to extend
 * @return Pointer to free blocks
 */
static void* extend_heap(size_t words) {
  size_t ext_size = ((words % 2) ? (words + 1) : words) * WORD_SIZE;
  if (ext_size < MIN_BLOCK_SIZE) ext_size = MIN_BLOCK_SIZE;
  char* bp;
  if ((bp = mem_sbrk(ext_size)) == ERRPTR) return NULL;
  PUT_PACK(GET_HEADER(bp), ext_size, BTAG_KEEP, 0);
  PUT_PACK(GET_FOOTER(bp), ext_size, GET_BTAG(GET_HEADER(bp)), 0);
  // epilogue header
  PUT_PACK(GET_HEADER(GET_NEXT_BLOCK(bp)), WORD_SIZE, BTAG_FREE, 1);
  PUT_FREE_BTAG(GET_HEADER(GET_NEXT_BLOCK(bp)));
  return coalesce(bp);
}

/**
 * @brief Merge two adjacent free blocks
 *
 * @param bp One of the block to be merged
 * @return Pointer to new merged block
 */
static void* coalesce(void* bp) {
  // WORD prev_alloc = GET_ALLOC(GET_FOOTER(GET_PREV_BLOCK(bp)));
  WORD prev_alloc = GET_BTAG(GET_HEADER(bp)) == BTAG_ALLOC;
  WORD next_alloc = GET_ALLOC(GET_HEADER(GET_NEXT_BLOCK(bp)));
  size_t size = GET_SIZE(GET_HEADER(bp));
  if (prev_alloc && next_alloc) {
    PUT_PACK(GET_FOOTER(bp), size, GET_BTAG(GET_HEADER(bp)), 0);
  } else if (prev_alloc && !next_alloc) {
    size_t next_size = GET_SIZE(GET_HEADER(GET_NEXT_BLOCK(bp)));
    size += next_size;
    seglist_remove(GET_NEXT_BLOCK(bp), next_size);
    PUT_PACK(GET_HEADER(bp), size, BTAG_KEEP, 0);
    PUT_PACK(GET_FOOTER(bp), size, GET_BTAG(GET_HEADER(bp)), 0);
  } else if (!prev_alloc && next_alloc) {
    size_t prev_size = GET_SIZE(GET_HEADER(GET_PREV_BLOCK(bp)));
    size += prev_size;
    seglist_remove(GET_PREV_BLOCK(bp), prev_size);
    bp = GET_PREV_BLOCK(bp);
    PUT_PACK(GET_HEADER(bp), size, BTAG_KEEP, 0);
    PUT_PACK(GET_FOOTER(bp), size, GET_BTAG(GET_HEADER(bp)), 0);
  } else {
    size_t next_size = GET_SIZE(GET_HEADER(GET_NEXT_BLOCK(bp)));
    size_t prev_size = GET_SIZE(GET_HEADER(GET_PREV_BLOCK(bp)));
    size += next_size + prev_size;
    seglist_remove(GET_NEXT_BLOCK(bp), next_size);
    seglist_remove(GET_PREV_BLOCK(bp), prev_size);
    bp = GET_PREV_BLOCK(bp);
    PUT_PACK(GET_HEADER(bp), size, BTAG_KEEP, 0);
    PUT_PACK(GET_FOOTER(bp), size, GET_BTAG(GET_HEADER(bp)), 0);
  }
  seglist_insert(bp, size);
  CHECK_HEAP();
  return bp;
}

/**
 * @brief Get an appropriate free block depends on @c size
 *
 * @param size The size of required block
 * @return Pointer to a fit block
 */
static void* find_fit(size_t size) {
  size_t index = seglist_get_index(size);
  void* fp = NULL;
  for (; index < SEGLIST_SIZE && (fp = seglist_find(index, size)) == NULL;
       index++)
    ;
  return fp;
}

/**
 * @brief Place allocated block inside a free block
 * Split a free block if available
 * @param ptr Where to place
 * @param alloc_size Allocated size
 */
static void place(void* ptr, size_t alloc_size) {
  size_t free_size = GET_SIZE(GET_HEADER(ptr));
  seglist_remove(ptr, free_size);
  ptrdiff_t diff = free_size - alloc_size;
  if (diff >= MIN_BLOCK_SIZE) {
    PUT_PACK(GET_HEADER(ptr), alloc_size, BTAG_KEEP, 1);
    void* bp = GET_NEXT_BLOCK(ptr);
    PUT_PACK(GET_HEADER(bp), diff, BTAG_ALLOC, 0);
    PUT_PACK(GET_FOOTER(bp), diff, BTAG_ALLOC, 0);
    seglist_insert(bp, diff);
  } else {
    PUT_PACK(GET_HEADER(ptr), free_size, BTAG_KEEP, 1);
    PUT_ALLOC_BTAG(GET_HEADER(GET_NEXT_BLOCK(ptr)));
  }
  CHECK_HEAP();
}

// Begin seglist helper functions

/**
 * @brief Determine which seglist to use base on @c size
 *
 * @param size size of a block
 * @return index to seglist
 */
static size_t seglist_get_index(size_t size) {
  switch (size) {
    case 0 ... 32:
      return 0;
    case 33 ... 64:
      return 1;
    case 65 ... 128:
      return 2;
    case 129 ... 256:
      return 3;
    case 257 ... 512:
      return 4;
    case 513 ... 1024:
      return 5;
    case 1025 ... 2048:
      return 6;
    case 2049 ... 4096:
      return 7;
    case 4097 ... 8192:
      return 8;
    case 8193 ... 16384:
      return 9;
    case 16385 ... 32768:
      return 10;
    case 32769 ... 65536:
      return 11;
    case 65537 ... 131072:
      return 12;
    case 131073 ... 262144:
      return 13;
    case 262145 ... 524288:
      return 14;
    case 524289 ... 1048576:
      return 15;
    default:
      return 16;
  }
}

/**
 * @brief Insert a free block to seglist
 * Insert to the head of a list
 * @param fp Pointer to free block
 * @param size Free block size
 */
static void seglist_insert(void* fp, size_t size) {
  size_t index = seglist_get_index(size);
  char* insert_pt = seglist[index];
  if (insert_pt) {  // list not empty
    SET_PREV_FREE(insert_pt, fp);
  }
  SET_NEXT_FREE(fp, insert_pt);
  SET_PREV_FREE(fp, NULL);
  seglist[index] = fp;
}

/**
 * @brief Remove a free block from seglist
 *
 * @param fp Pointer to free block
 * @param size Free block size
 */
static void seglist_remove(void* fp, size_t size) {
  size_t index = seglist_get_index(size);
  char* next = GET_NEXT_FREE(fp);
  char* prev = GET_PREV_FREE(fp);
  if (prev) {
    SET_NEXT_FREE(prev, next);
  } else {
    seglist[index] = next;
  }
  if (next) {
    SET_PREV_FREE(next, prev);
  }
}

/**
 * @brief Find appropriate free block in seglist
 * The minimum block contains @c size bytes
 * @param index Index of seglist or @c SEGLIST_AUTO
 * @param size Size of target
 * @return Pointer to the fit block, @c NULL if fit not fround
 */
static void* seglist_find(size_t index, size_t size) {
  if (index == SEGLIST_AUTO) index = seglist_get_index(size);
  void* fp = seglist[index];
  while (fp) {
    if (size <= GET_SIZE(GET_HEADER(fp))) return fp;
    fp = GET_NEXT_FREE(fp);
  }
  return NULL;
}

// End seglist helper functions

// Begin debug (Heap Checker) functions

/**
 * @brief Determine whether the pointer is in the heap.
 *
 * @param p The pointer
 * @return Whether @c p is in the heap
 */
static int in_heap(const void* p) {
  return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/**
 * @brief Determine whether the pointer is aligned with ALIGNMENT
 *
 * @param p The pointer
 * @return Whether @c p is aligned
 */
static int aligned(const void* p) { return (size_t)ALIGN(p) == (size_t)p; }

/**
 * @brief Heap consistency checker
 * Scans the heap and checks it for correctness, call it by @c CHECK_HEAP
 * **For Teacher Assistants / Instructors**
 *   I use this function instead of default @c mm_checkheap
 *   because I prefer passing @c __func__ than @c __LINE__ .
 * @param func Pass @c __func__ into it for debugging
 * @param lineno Pass @c __LINE__ macro into it for debugging
 */
void my_checkheap(const char* func, int lineno) {
// Print original caller
#define ch_printf(FORMAT, ...) \
  dbg_printf("%s(%d): " FORMAT, func, lineno, ##__VA_ARGS__)
  void* bp;
  void* header;
  void* footer;
  void* heap_end = (char*)mem_heap_hi() + 1;
  void* prev;
  if ((heap_begin - DWORD_SIZE - (char*)(mem_heap_lo())) !=
      SEGLIST_SIZE * DWORD_SIZE) {
    ch_printf("Seglist pointers don't have enough space.");
  }
  // Prologue checking
  bp = heap_begin;
  header = GET_HEADER(bp);
  footer = GET_FOOTER(bp);
  if (GET_SIZE(header) != DWORD_SIZE || GET_ALLOC(header) != 1) {
    ch_printf("Prologue block smashed: wrong size (header)");
    ch_printf("Prologue header: " PACK_FMT, PACK_ARG(header));
    exit(EXIT_FAILURE);
  }
  if (GET_SIZE(footer) != DWORD_SIZE || GET_ALLOC(footer) != 1) {
    ch_printf("Prologue block smashed: wrong size (footer)");
    ch_printf("Prologue footer: " PACK_FMT, PACK_ARG(footer));
    exit(EXIT_FAILURE);
  }
  // Epilogue checking
  bp = heap_end;
  header = GET_HEADER(bp);
  footer = GET_FOOTER(bp);
  if (GET_SIZE(header) != 0 || GET_ALLOC(header) != 1) {
    ch_printf("Epilogue block smashed: wrong size");
    ch_printf("Epilogue header: " PACK_FMT, PACK_ARG(header));
    exit(EXIT_FAILURE);
  }
  // Alignment checking
  for (bp = heap_begin; bp != heap_end; bp = GET_NEXT_BLOCK(bp)) {
    if (!in_heap(bp)) {
      ch_printf("Block %p not in heap (%p:%p): ", bp, mem_heap_hi(),
                mem_heap_lo());
      ch_printf("Header: " PACK_FMT, PACK_ARG(GET_HEADER(bp)));
      exit(EXIT_FAILURE);
    }
    if (!aligned(bp)) {
      ch_printf("Block %p not aligned", bp);
      ch_printf("Header: " PACK_FMT, PACK_ARG(GET_HEADER(bp)));
      exit(EXIT_FAILURE);
    }
  }
  // H/F checking
  prev = heap_begin;
  for (bp = GET_NEXT_BLOCK(heap_begin); bp != heap_end;
       prev = bp, bp = GET_NEXT_BLOCK(bp)) {
    header = GET_HEADER(bp);
    if (!GET_ALLOC(header)) {
      // free block
      footer = GET_FOOTER(bp);
      if (GET_SIZE(header) != GET_SIZE(footer) ||
          GET_ALLOC(header) != GET_ALLOC(footer) ||
          GET_BTAG(header) != GET_BTAG(footer)) {
        ch_printf("Block %p H/F mismatch:", bp);
        ch_printf("Header: " PACK_FMT, PACK_ARG(header));
        ch_printf("Footer: " PACK_FMT, PACK_ARG(footer));
        exit(EXIT_FAILURE);
      }
    }
    if (GET_SIZE(header) < MIN_BLOCK_SIZE) {
      ch_printf("Block %p too small: ", bp);
      ch_printf("Header: " PACK_FMT, PACK_ARG(header));
      exit(EXIT_FAILURE);
    }
    if (!!GET_BTAG(header) != GET_ALLOC(GET_HEADER(prev))) {
      ch_printf("BTAG of block %p doesn't match previous block ALLOC:", bp);
      ch_printf("Current(%p) header: " PACK_FMT, bp, PACK_ARG(GET_HEADER(bp)));
      ch_printf("Previous(%p) header: " PACK_FMT, prev,
                PACK_ARG(GET_HEADER(prev)));
      exit(EXIT_FAILURE);
    }
  }
  // Coalescing checking
  prev = heap_begin;
  for (bp = GET_NEXT_BLOCK(heap_begin); bp != heap_end;
       prev = bp, bp = GET_NEXT_BLOCK(bp)) {
    if (!GET_ALLOC(GET_HEADER(prev)) && !GET_ALLOC(GET_HEADER(bp))) {
      ch_printf("Adjacent free block %p and %p", prev, bp);
      ch_printf("%p header: " PACK_FMT, prev, PACK_ARG(GET_HEADER(prev)));
      ch_printf("%p header: " PACK_FMT, bp, PACK_ARG(GET_HEADER(prev)));
      exit(EXIT_FAILURE);
    }
  }
  // Seglist checking
  for (int i = 0; i < SEGLIST_SIZE; i++) {
    bp = seglist[i];
    if (bp && !in_heap(bp)) {
      ch_printf("seglist[%d] (%p) head not in heap.", i, bp);
      exit(EXIT_FAILURE);
    }
  }
  for (bp = heap_begin; bp != heap_end; bp = GET_NEXT_BLOCK(bp)) {
    if (!GET_ALLOC(GET_HEADER(bp))) {
      if (GET_NEXT_FREE(bp) && !in_heap(GET_NEXT_FREE(bp))) {
        ch_printf("Free block %p 's next (%p) is not in heap.", bp,
                  GET_NEXT_FREE(bp));
        exit(EXIT_FAILURE);
      }
      if (GET_PREV_FREE(bp) && !in_heap(GET_PREV_FREE(bp))) {
        ch_printf("Free block %p 's prev (%p) is not in heap.", bp,
                  GET_PREV_FREE(bp));
        exit(EXIT_FAILURE);
      }
    }
  }
  for (int i = 0; i < SEGLIST_SIZE; i++) {
    void* bp = seglist[i];
    while (bp) {
      bp = GET_NEXT_FREE(bp);
      if (bp && GET_NEXT_FREE(GET_PREV_FREE(bp)) != bp) {
        ch_printf("Mistaken linking between free blocks: ");
        ch_printf("bp            : %p", bp);
        ch_printf("bp->prev      : %p", GET_PREV_FREE(bp));
        ch_printf("bp->prev->next: %p", GET_NEXT_FREE(GET_PREV_FREE(bp)));
        exit(EXIT_FAILURE);
      }
    }
  }
#undef ch_printf
}

// End debug functions
