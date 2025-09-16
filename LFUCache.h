#pragma once

#include <unordered_map>
#include <list>
#include <memory>
#include "CachePolicy.h"

// LFU缓存实现
template<typename T>
class LFUCache {
private:
    std::unordered_map<std::string, T> cache_map;
    std::unordered_map<std::string, int> frequency_map;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> access_time_map;
    size_t capacity;

public:
    explicit LFUCache(size_t cap) : capacity(cap) {}
    
    bool get(const std::string& key, T& value) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            value = it->second;
            frequency_map[key]++;
            access_time_map[key] = std::chrono::steady_clock::now();
            return true;
        }
        return false;
    }
    
    void put(const std::string& key, const T& value) {
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            // 更新值
            it->second = value;
            frequency_map[key]++;
            access_time_map[key] = std::chrono::steady_clock::now();
            return;
        }
        
        // 添加新元素
        if (cache_map.size() >= capacity) {
            // 找到频率最低且最久未使用的元素
            std::string evict_key;
            int min_freq = std::numeric_limits<int>::max();
            auto oldest_time = std::chrono::steady_clock::now();
            
            for (const auto& freq_pair : frequency_map) {
                const std::string& k = freq_pair.first;
                int freq = freq_pair.second;
                
                if (freq < min_freq || (freq == min_freq && access_time_map[k] < oldest_time)) {
                    min_freq = freq;
                    oldest_time = access_time_map[k];
                    evict_key = k;
                }
            }
            
            // 移除选中的元素
            cache_map.erase(evict_key);
            frequency_map.erase(evict_key);
            access_time_map.erase(evict_key);
        }
        
        // 添加新元素
        cache_map[key] = value;
        frequency_map[key] = 1;
        access_time_map[key] = std::chrono::steady_clock::now();
    }
    
    size_t size() const {
        return cache_map.size();
    }
};