/**
 * @file cache.h
 * @author Guyutongxue (1900012983@pku.edu.cn)
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

// Recommended max cache size
// static constexpr const std::size_t MAX_CACHE_SIZE{1049000};

/**
 * @brief The maximum size of each cache object
 *
 */
static constexpr const std::size_t MAX_OBJECT_SIZE{102400};

/**
 * @brief The maximum number of cache block
 *
 */
static constexpr const std::size_t CACHE_BLOCK_NUM{10};

/**
 * @brief Our caching object is a byte-array
 *
 */
using CacheContent = std::array<char, MAX_OBJECT_SIZE>;

/**
 * @brief Set content to cache
 *
 * @param uri The URI of this cache
 * @param content THe content of this cache
 */
void cache_set(const std::string& uri, const CacheContent& content);

/**
 * @brief Get content from cache
 *
 * @param uri Which cache
 * @return If cache exists, return corresponding cache content; else return
 * std::nullopt
 */
std::optional<const CacheContent> cache_get(const std::string& uri);

#endif  // CACHE_H