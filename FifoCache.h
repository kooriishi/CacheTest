#pragma once

#include <unordered_map>
#include <list>
#include <memory>
#include "CachePolicy.h"

// FIFO缓存实现
template<typename T>
class FIFOCache {
private:
    std::unordered_map<std::string, std::shared_ptr<CacheItem<T>>> cache_map;
    std::list<std::shared_ptr<CacheItem<T>>> cache_list;
    size_t capacity;

public:
    explicit FIFOCache(size_t cap) : capacity(cap) {}
    
    bool get(const std::string& key, T& value) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            value = it->second->value;
            return true;
        }
        return false;
    }
    
    void put(const std::string& key, const T& value) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            // 更新值
            it->second->value = value;
            return;
        }
        
        // 添加新元素
        if (cache_map.size() >= capacity) {
            // 移除最老的元素
            auto oldest = cache_list.front();
            cache_list.pop_front();
            cache_map.erase(oldest->key);
        }
        
        auto item = std::make_shared<CacheItem<T>>(key, value);
        cache_list.push_back(item);
        cache_map[key] = item;
    }
    
    size_t size() const {
        return cache_map.size();
    }
};