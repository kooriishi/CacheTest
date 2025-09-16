#pragma once

#include <unordered_map>
#include <list>
#include <memory>
#include "CachePolicy.h"


// ARC缓存实现
template<typename T>
class ARCCache {
private:
    struct ARCItem {
        std::string key;
        T value;
        bool is_ghost;  // 标记是否为幽灵条目
        
        ARCItem(const std::string& k, const T& v, bool ghost = false) 
            : key(k), value(v), is_ghost(ghost) {}
    };
    
    // 四个列表
    std::list<std::shared_ptr<ARCItem>> t1, t2, b1, b2;
    std::unordered_map<std::string, typename std::list<std::shared_ptr<ARCItem>>::iterator> t1_map, t2_map, b1_map, b2_map;
    
    size_t capacity;
    size_t p;  // t1列表的目标大小
    
    void replace(bool is_b2) {
        if (!t1.empty() && (t1.size() > p || (is_b2 && t1.size() == p))) {
            // 从t1移除最近最少使用的项到b1
            auto item = t1.front();
            t1.pop_front();
            t1_map.erase(item->key);
            
            // 如果不是幽灵条目，才添加到b1
            if (!item->is_ghost) {
                item->is_ghost = true;
                b1.push_back(item);
                b1_map[item->key] = --b1.end();
            }
        } else if (!t2.empty()) {
            // 从t2移除最近最少使用的项到b2
            auto item = t2.front();
            t2.pop_front();
            t2_map.erase(item->key);
            
            // 如果不是幽灵条目，才添加到b2
            if (!item->is_ghost) {
                item->is_ghost = true;
                b2.push_back(item);
                b2_map[item->key] = --b2.end();
            }
        }
    }
    
    void removeGhostEntries() {
        // 清理幽灵条目以保持容量限制
        while (b1.size() + b2.size() >= capacity) {
            if (!b1.empty() && (b1.size() > b2.size() || (b1.size() == b2.size() && !b2.empty()))) {
                auto item = b1.front();
                b1.pop_front();
                b1_map.erase(item->key);
            } else if (!b2.empty()) {
                auto item = b2.front();
                b2.pop_front();
                b2_map.erase(item->key);
            }
        }
    }

public:
    explicit ARCCache(size_t cap) : capacity(cap), p(0) {}
    
    bool get(const std::string& key, T& value) {
        // 检查T1
        auto it1 = t1_map.find(key);
        if (it1 != t1_map.end()) {
            auto item = *it1->second;
            if (!item->is_ghost) {
                value = item->value;
                // 移动到T2
                t1.erase(it1->second);
                t1_map.erase(key);
                t2.push_back(item);
                t2_map[key] = --t2.end();
                return true;
            }
        }
        
        // 检查T2
        auto it2 = t2_map.find(key);
        if (it2 != t2_map.end()) {
            auto item = *it2->second;
            if (!item->is_ghost) {
                value = item->value;
                // 移动到T2尾部(最近使用)
                t2.erase(it2->second);
                t2_map.erase(key);
                t2.push_back(item);
                t2_map[key] = --t2.end();
                return true;
            }
        }
        
        return false;
    }
    
    void put(const std::string& key, const T& value) {
        // 检查是否在T1中
        auto it1 = t1_map.find(key);
        if (it1 != t1_map.end()) {
            auto item = *it1->second;
            if (!item->is_ghost) {
                // 更新值并移动到T2
                item->value = value;
                t1.erase(it1->second);
                t1_map.erase(key);
                t2.push_back(item);
                t2_map[key] = --t2.end();
                return;
            } else {
                // 在B1中，增加p
                p = std::min(p + 1, capacity);
                // 移除幽灵条目
                t1.erase(it1->second);
                t1_map.erase(key);
            }
        }
        
        // 检查是否在T2中
        auto it2 = t2_map.find(key);
        if (it2 != t2_map.end()) {
            auto item = *it2->second;
            if (!item->is_ghost) {
                // 更新值
                item->value = value;
                // 移动到T2尾部
                t2.erase(it2->second);
                t2_map.erase(key);
                t2.push_back(item);
                t2_map[key] = --t2.end();
                return;
            } else {
                // 在B2中，减少p
                if (b1.size() > 0) {
                    p = (p > (b2.size() / b1.size() + 1)) ? (p - (b2.size() / b1.size() + 1)) : 0;
                } else {
                    p = (p > 1) ? (p - 1) : 0;
                }
                // 移除幽灵条目
                t2.erase(it2->second);
                t2_map.erase(key);
            }
        }
        
        // 检查是否在B1中
        auto ib1 = b1_map.find(key);
        if (ib1 != b1_map.end()) {
            // 增加p
            size_t delta = 1;
            if (b2.size() > 0) {
                delta = std::max(b2.size() / b1.size(), size_t(1));
            }
            p = std::min(p + delta, capacity);
            // 替换
            replace(false);
            // 移除B1中的条目
            b1.erase(ib1->second);
            b1_map.erase(key);
            // 添加到T2
            auto item = std::make_shared<ARCItem>(key, value);
            t2.push_back(item);
            t2_map[key] = --t2.end();
            return;
        }
        
        // 检查是否在B2中
        auto ib2 = b2_map.find(key);
        if (ib2 != b2_map.end()) {
            // 减少p
            size_t delta = 1;
            if (b1.size() > 0) {
                delta = std::max(b1.size() / b2.size(), size_t(1));
            }
            p = (p > delta) ? (p - delta) : 0;
            // 替换
            replace(true);
            // 移除B2中的条目
            b2.erase(ib2->second);
            b2_map.erase(key);
            // 添加到T2
            auto item = std::make_shared<ARCItem>(key, value);
            t2.push_back(item);
            t2_map[key] = --t2.end();
            return;
        }
        
        // 新条目
        if (t1.size() + b1.size() == capacity) {
            // L1已满
            if (t1.size() < capacity) {
                // 移除B1中的一个条目
                if (!b1.empty()) {
                    auto item = b1.front();
                    b1.pop_front();
                    b1_map.erase(item->key);
                }
                replace(false);
            } else {
                // 移除T1中的一个条目
                auto item = t1.front();
                t1.pop_front();
                t1_map.erase(item->key);
            }
        } else {
            // L1和L2总和已满
            size_t total = t1.size() + t2.size() + b1.size() + b2.size();
            if (total >= capacity) {
                if (t1.size() + t2.size() < 2 * capacity) {
                    removeGhostEntries();
                    replace(false);
                } else {
                    // 移除T1或T2中的一个条目
                    if (!t1.empty()) {
                        auto item = t1.front();
                        t1.pop_front();
                        t1_map.erase(item->key);
                    } else if (!t2.empty()) {
                        auto item = t2.front();
                        t2.pop_front();
                        t2_map.erase(item->key);
                    }
                }
            }
        }
        
        // 添加到T1
        auto item = std::make_shared<ARCItem>(key, value);
        t1.push_back(item);
        t1_map[key] = --t1.end();
    }
    
    size_t size() const {
        return t1_map.size() + t2_map.size();
    }
};