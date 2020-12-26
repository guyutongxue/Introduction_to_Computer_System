/**
 * @file cache.cpp
 * @author Guyutongxue (1900012983@pku.edu.cn)
 * @brief The implementation of proxy cache
 * This cache use LRU eviction policy.
 * It use std::mutex for preventing simultaneous accessing.
 * ( @c std::mutex::lock & @c std::mutex::unlock is
 *   identical to @c csapp::P & @c csapp::V .)
 * @version 0.1
 * @date 2020-12-26
 *
 * @copyright Copyright (c) 2020 Guyutongxue
 *
 */

#include "./cache.h"

/**
 * @brief The structure storing cache object
 *
 */
struct CacheBlock {
  CacheContent content{};             ///< An byte-array storing cache object
  std::string uri{};                  ///< URI of this cache
  int lru{0};                         ///< Last used time, updated after setting
  bool is_empty{true};                ///< Whether this block is empty
  mutable int read_cnt{0};            ///< How many simutaneously reader threads
  mutable std::mutex cache_mutex;     ///< The mutex for the whole block
  mutable std::mutex read_cnt_mutex;  ///< The mutex for @c read_cnt
};

/**
 * @brief Cache for proxy
 *
 */
std::array<CacheBlock, CACHE_BLOCK_NUM> cache{};

/**
 * @brief Current time
 *
 */
std::size_t current_lru{0};

/**
 * @brief Things to do before reading cache
 *
 * @param i The cache to read
 */
static void ante_read(const CacheBlock& i) {
  i.read_cnt_mutex.lock();
  if (i.read_cnt == 0) i.cache_mutex.lock();
  i.read_cnt++;
  i.read_cnt_mutex.unlock();
}

/**
 * @brief Things to do after reading cache
 *
 * @param i The cache to read
 */
static void post_read(const CacheBlock& i) {
  i.read_cnt_mutex.lock();
  i.read_cnt--;
  if (i.read_cnt == 0) i.cache_mutex.unlock();
  i.read_cnt_mutex.unlock();
}

/**
 * @brief Things to do before writing cache
 *
 * @param i The cache to write
 */
static void ante_write(CacheBlock& i) { i.cache_mutex.lock(); }

/**
 * @brief Things to do after writing cache
 *
 * @param i The cache to write
 */
static void post_write(CacheBlock& i) { i.cache_mutex.unlock(); }

std::optional<const CacheContent> cache_get(const std::string& uri) {
  for (const auto& i : cache) {
    ante_read(i);
    if (!i.is_empty && i.uri == uri) {
      CacheContent content = i.content;
      post_read(i);
      return content;
    }
    post_read(i);
  }
  return std::nullopt;
}

static CacheBlock& cache_eviction() {
  return *std::min_element(cache.begin(), cache.end(),
                           [](const CacheBlock& a, const CacheBlock& b) {
                             ante_read(a);
                             ante_read(b);
                             bool result;
                             if (a.is_empty)
                               result = true;
                             else if (b.is_empty)
                               result = false;
                             else
                               result = a.lru < b.lru;
                             post_read(a);
                             post_read(b);
                             return result;
                           });
}

void cache_set(const std::string& uri, const CacheContent& content) {
  CacheBlock& target = cache_eviction();
  ante_write(target);
  target.uri = uri;
  target.content = content;
  target.is_empty = false;
  target.lru = current_lru++;
  post_write(target);
}