/**
 * @file mm.c
 * @author Guyutongxue (1900012983@pku.edu.cn)
 * @brief Handin file of MallocLab
 * @version 0.1
 * @date 2020-12-18 ~
 *
 * @copyright Copyright (c) 2020 Guyutongxue
 *
 */

#include <assert.h>
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

// DEBUG defined in Makefile
#ifdef DEBUG
#define dbg_printf(FORMAT, ...) \
  printf("%s(%d): " FORMAT, __func__, __LINE__, ##__VA_ARGS__)
#else
#define dbg_printf(...)
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

// rounds up to the nearest multiple of ALIGNMENT
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT - 1)) & ~0x7)

/**
 * @brief Initialize dynamic allocator
 *
 * @return -1 on error, 0 on success
 */
int mm_init(void) {
  dbg_printf("Hello\n");
  return 0;
}

/**
 * @brief Allocate @c size bytes of memory.
 *
 * @param size The size of allocated memory.
 * @return Pointer to allocated memory with ALIGNMENT
 */
void* malloc(size_t size) { return NULL; }

/**
 * @brief Free a block allocated by @c malloc , @c realloc or @c calloc
 *
 * @param ptr Pointer to the block will be freed
 */
void free(void* ptr) {
  if (!ptr) return;
}

/**
 * @brief Re-allocation function
 * Re-allocate the previously allocated block in @c oldptr ,
 * making the new block @c size bytes long.
 * @param oldptr Pointer to previously allocated block
 * @param size New block size
 * @return Point to new allocated block
 */
void* realloc(void* oldptr, size_t size) { return NULL; }

/**
 * @brief Allocation with initialization.
 * Allocate @c nmemb elements of @c size bytes each, all initialized to 0.
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return Pointer to first element allocated
 */
void* calloc(size_t nmemb, size_t size) { return NULL; }

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void* p) {
  return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void* p) { return (size_t)ALIGN(p) == (size_t)p; }

/**
 * @brief Heap consistency checker
 * Scans the heap and checks it for correctness
 * @param lineno Pass __LINE__ macro into it for debugging
 */
void mm_checkheap(int lineno) {}
