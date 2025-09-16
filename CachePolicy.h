#pragma once

#include <string>
#include <chrono>

// 缓存项结构
template<typename T>
struct CacheItem {
    std::string key;
    T value;
    int frequency;  // 用于LFU
    std::chrono::steady_clock::time_point last_accessed; // 用于LRU
    
    CacheItem(const std::string& k, const T& v) : key(k), value(v), frequency(1) {
        last_accessed = std::chrono::steady_clock::now();
    }
};