/**
 * @file cache.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2020-12-26
 *
 * @copyright Copyright (c) 2020
 *
 */

#include "./cache.h"

std::array<CacheBlock, CACHE_BLOCK_NUM> cache{};
std::size_t current_lru{0};

static void reader_prev(const CacheBlock& i) {
  i.read_cnt_mutex.lock();
  if (i.read_cnt == 0) i.cache_mutex.lock();
  i.read_cnt++;
  i.read_cnt_mutex.unlock();
}

static void reader_post(const CacheBlock& i) {
  i.read_cnt_mutex.lock();
  i.read_cnt--;
  if (i.read_cnt == 0) i.cache_mutex.unlock();
  i.read_cnt_mutex.unlock();
}

static void writer_prev(CacheBlock& i) { i.cache_mutex.lock(); }

static void writer_post(CacheBlock& i) { i.cache_mutex.unlock(); }

std::optional<const CacheContent> cache_get(const std::string& uri) {
  for (const auto& i : cache) {
    reader_prev(i);
    if (!i.is_empty && i.uri == uri) {
      CacheContent content = i.content;
      reader_post(i);
      return content;
    }
    reader_post(i);
  }
  return std::optional<const CacheContent>{};
}

static CacheBlock& cache_eviction() {
  return *std::min_element(cache.begin(), cache.end(),
                           [](const CacheBlock& a, const CacheBlock& b) {
                             reader_prev(a);
                             reader_prev(b);
                             bool result;
                             if (a.is_empty) result = true;
                             else if (b.is_empty) result = false;
                             else result = a.lru < b.lru;
                             reader_post(a);
                             reader_post(b);
                             return result;
                           });
}

void cache_set(const std::string& uri, const CacheContent& content) {
  CacheBlock& target = cache_eviction();
  writer_prev(target);
  target.uri = uri;
  target.content = content;
  target.is_empty = false;
  target.lru = current_lru++;
  writer_post(target);
}