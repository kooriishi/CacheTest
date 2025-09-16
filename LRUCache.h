#pragma once

#include <unordered_map>
#include <list>
#include <memory>
#include "CachePolicy.h"

// LRU缓存实现
template<typename T>
class LRUCache {
private:
    std::unordered_map<std::string, typename std::list<std::shared_ptr<CacheItem<T>>>::iterator> cache_map;
    std::list<std::shared_ptr<CacheItem<T>>> cache_list;
    size_t capacity;

public:
    explicit LRUCache(size_t cap) : capacity(cap) {}
    
    bool get(const std::string& key, T& value) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            auto item = *it->second;
            value = item->value;
            
            // 更新访问时间并移到链表尾部
            item->last_accessed = std::chrono::steady_clock::now();
            cache_list.erase(it->second);
            cache_list.push_back(item);
            cache_map[key] = --cache_list.end();
            
            return true;
        }
        return false;
    }
    
    void put(const std::string& key, const T& value) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            // 更新值和访问时间
            auto item = *it->second;
            item->value = value;
            item->last_accessed = std::chrono::steady_clock::now();
            
            cache_list.erase(it->second);
            cache_list.push_back(item);
            cache_map[key] = --cache_list.end();
            return;
        }
        
        // 添加新元素
        if (cache_map.size() >= capacity) {
            // 移除最久未使用的元素
            auto oldest = cache_list.front();
            cache_map.erase(oldest->key);
            cache_list.pop_front();
        }
        
        auto item = std::make_shared<CacheItem<T>>(key, value);
        cache_list.push_back(item);
        cache_map[key] = --cache_list.end();
    }
    
    size_t size() const {
        return cache_map.size();
    }
};