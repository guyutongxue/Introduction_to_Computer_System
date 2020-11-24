/**
 * @file csim.c
 * @author Guyutongxue (1900012983@pku.edu.cn)
 * @brief This file is part of CacheLab, part A
 * @version 0.1
 * @date 2020-11-24
 *
 * @copyright Copyright (c) 2020
 *
 */

// This file should compiled under C99 Standard.
// This file follows Google C++ Style Guide.

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GNU Getopt Library
#include <getopt.h>
#include <unistd.h>

// CacheLab utility
#include "cachelab.h"

/**
 * @brief Structure for saving info about a cache line
 * 
 */
struct CacheLine {
  bool v;                       ///< Is line available
  unsigned recently_used_time;  ///< The most recently used time of this line
  uint64_t tag;                 ///< The 'tag' part
};

/// Required info of cache
unsigned B = 0, E = 0, S = 0;
unsigned b = 0, s = 0;

/// Path to trace file
char trace_file[1000];

/// Cache
struct CacheLine** cache = NULL;

/// Helper variable for deviding address to index part
uint64_t set_index_mask;

/// What we want
unsigned eviction_count, hit_count, miss_count;

/// Global counter, determine how to evict line
unsigned lru_counter = 1;

/// Is verbose output
bool verbosity;

/**
 * @brief Initialize cache
 *
 */
void InitCache(void) {
  cache = malloc(sizeof(struct CacheLine*) * S);
  for (unsigned i = 0; i < S; i++) {
    cache[i] = malloc(sizeof(struct CacheLine) * E);
    for (unsigned j = 0; j < E; j++) {
      cache[i][j] = (struct CacheLine){0, 0, 0};
    }
  }
  set_index_mask = ~(-1l << s);
}

/**
 * @brief Access data at certain address
 * ...while dealing with cache and statstics stuff
 * @param address Where to access
 */
void AccessData(uint64_t address) {
  uint64_t tag_part = address >> (s + b);
  uint64_t index_part = (address >> b) & set_index_mask;
  struct CacheLine* target_group = cache[index_part];
  for (unsigned i = 0; i < E; i++) {
    if (target_group[i].v && target_group[i].tag == tag_part) {
      hit_count++;
      if (verbosity) {
        printf("%s", "hit ");
      }
      target_group[i].recently_used_time = lru_counter++;
      return;
    }
  }
  miss_count++;
  if (verbosity) {
    printf("%s", "miss ");
  }
  unsigned min_index = 0;
  unsigned min_time = lru_counter;
  for (unsigned i = 0; i < E; i++) {
    if (target_group[i].recently_used_time < min_time) {
      min_time = target_group[i].recently_used_time;
      min_index = i;
    }
  }
  if (target_group[min_index].v) {
    eviction_count++;
    if (verbosity) {
      printf("%s", "eviction ");
    }
  }
  target_group[min_index].v = true;
  target_group[min_index].tag = tag_part;
  target_group[min_index].recently_used_time = lru_counter++;
}

/**
 * @brief Read and parse *.trace file
 *
 * @param filepath The path to *.trace file
 */
void ReplayTrace(char* filepath) {
  char buf[1000] = {0};
  FILE* file = fopen(filepath, "r");
  if (!file) {
    fprintf(stderr, "%s: %s\n", filepath, strerror(errno));
    exit(EXIT_FAILURE);
  } else {
    char type;
    uint64_t address;
    unsigned bytes;
    while (fgets(buf, 1000, file) != NULL) {
      type = buf[1];
      switch (type) {
        case 'S':
        case 'L':
        case 'M':
          sscanf(buf + 2, "%lx, %u", &address, &bytes);
          if (verbosity) {
            printf("%c %lx, %u ", type, address, bytes);
          }
          AccessData(address);
          // Double access for 'M'
          if (type == 'M') {
            AccessData(address);
          }
          if (verbosity) {
            putchar('\n');
          }
          break;
        default:
          break;
      }
    }
    fclose(file);
  }
  printf("%lx\n", set_index_mask);
}

/**
 * @brief Destroy cache
 *
 */
void FreeCache(void) {
  for (unsigned i = 0; i < S; i++) {
    free(cache[i]);
    cache[i] = NULL;
  }
  free(cache);
  cache = NULL;
}

/**
 * @brief Print usage of this program
 *
 * @param name The name of calling this program
 */
void printUsage(char* name) {
  printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", name);
  puts("Options:");
  puts("  -h         Print this help message.");
  puts("  -v         Optional verbose flag.");
  puts("  -s <num>   Number of set index bits.");
  puts("  -E <num>   Number of lines per set.");
  puts("  -b <num>   Number of block offset bits.");
  puts("  -t <file>  Trace file.");
  puts("\nExamples:");
  printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", "\nExamples:");
  printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n",
         "  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n");
  exit(0);
}

int main(int argc, char** argv) {
  int c;
  extern char* optarg;
  while ((c = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
    switch (c) {
      case 's':
        sscanf(optarg, "%u", &s);
        break;
      case 'E':
        sscanf(optarg, "%u", &E);
        break;
      case 'b':
        sscanf(optarg, "%u", &b);
        break;
      case 't':
        strcpy(trace_file, optarg);
        break;
      case 'v':
        verbosity = true;
        break;
      case 'h':
        printUsage(argv[0]);
        exit(EXIT_SUCCESS);
      case '?':
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
      default:
        abort();
    }
  }
  if (s == 0 || E == 0 || b == 0 || !trace_file[0]) {
    fprintf(stderr, "%s: Failed fetching command line argument.\n", argv[0]);
    printUsage(argv[0]);
    exit(EXIT_FAILURE);
  }
  S = 1 << s;
  B = 1 << b;
  InitCache();
  ReplayTrace(trace_file);
  FreeCache();
  printSummary(hit_count, miss_count, eviction_count);
  return EXIT_SUCCESS;
}