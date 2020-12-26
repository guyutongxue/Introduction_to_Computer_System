/**
 * @file cache.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2020-12-26
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#ifndef CACHE_H
#define CACHE_H

#include <algorithm>
#include <array>
#include <cstdlib>
#include <mutex>
#include <optional>

// Recommended max cache and object sizes
static constexpr const std::size_t MAX_CACHE_SIZE{1049000};
static constexpr const std::size_t MAX_OBJECT_SIZE{102400};
static constexpr const std::size_t CACHE_BLOCK_NUM{10};

using CacheContent = std::array<char, MAX_OBJECT_SIZE>;

struct CacheBlock {
  CacheContent content{};
  std::string uri{};
  int lru{0};
  bool is_empty{true};
  mutable int read_cnt{0};
  mutable std::mutex cache_mutex;
  mutable std::mutex read_cnt_mutex;
};

extern std::array<CacheBlock, CACHE_BLOCK_NUM> cache;
extern std::size_t current_lru;

void cache_set(const std::string& uri, const CacheContent& content);
std::optional<const CacheContent> cache_get(const std::string& uri);

#endif // CACHE_H