#pragma once

#include <vector>
#include <unordered_map>
#include <list>
#include <mutex>
#include <memory>

#include "Types.h"

#include <spdlog/spdlog.h>

namespace motioncam {

class LRUCache {
public:
    explicit LRUCache(size_t maxSize) : mMaxSize(maxSize), mCurrentSize(0) {}

    // Get value from cache, returns nullptr if not found
    std::shared_ptr<std::vector<char>> get(const Entry& key) {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = mCacheMap.find(key);
        if (it == mCacheMap.end()) {
            // Cache miss
            return nullptr;
        }

        // Cache hit, move to front of list (most recently used)
        mCacheList.splice(mCacheList.begin(), mCacheList, it->second);

        return it->second->second;
    }

    // Add or update value in cache
    void put(const Entry& key, std::shared_ptr<std::vector<char>> value) {
        std::lock_guard<std::mutex> lock(mMutex);

        size_t valueSize = value->size();

        // Check if key already exists in cache
        auto it = mCacheMap.find(key);

        if (it != mCacheMap.end()) {
            // Update value
            size_t oldSize = it->second->second->size();
            mCurrentSize -= oldSize;
            mCurrentSize += valueSize;

            // Move to front and update
            mCacheList.splice(mCacheList.begin(), mCacheList, it->second);
            it->second->second = value;
        }
        else {
            // New entry

            // If adding this would exceed max size, remove older entries
            while (!mCacheList.empty() && (mCurrentSize + valueSize > mMaxSize)) {
                auto last = mCacheList.back();
                mCurrentSize -= last.second->size();
                mCacheMap.erase(last.first);
                mCacheList.pop_back();
            }

            // If the single item is too large for the cache, don't add it
            if (valueSize > mMaxSize) {
                return;
            }

            // Add new entry
            mCacheList.emplace_front(key, value);
            mCacheMap[key] = mCacheList.begin();
            mCurrentSize += valueSize;
        }

        spdlog::debug("Cache size is {} bytes", mCurrentSize);
    }

    // Remove an entry from the cache
    void remove(const Entry& key) {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = mCacheMap.find(key);

        if (it != mCacheMap.end()) {
            mCurrentSize -= it->second->second->size();
            mCacheList.erase(it->second);
            mCacheMap.erase(it);
        }
    }

    // Clear the cache
    void clear() {
        std::lock_guard<std::mutex> lock(mMutex);

        mCacheMap.clear();
        mCacheList.clear();
        mCurrentSize = 0;
    }

    // Get current size
    size_t size() const {
        std::lock_guard<std::mutex> lock(mMutex);

        return mCurrentSize;
    }

    // Get maximum size
    size_t capacity() const {
        return mMaxSize;
    }

private:
    using CacheItem = std::pair<Entry, std::shared_ptr<std::vector<char>>>;
    using CacheList = std::list<CacheItem>;
    using CacheMap = std::unordered_map<Entry, typename CacheList::iterator, Entry::Hash>;

    CacheList mCacheList; // List of cache entries, most recently used at the front
    CacheMap mCacheMap;   // Map from key to list iterator
    size_t mMaxSize;      // Maximum cache size in bytes
    size_t mCurrentSize;  // Current cache size in bytes
    mutable std::mutex mMutex; // Mutex for thread safety
};

}
