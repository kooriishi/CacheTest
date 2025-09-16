#include <iostream>
#include <chrono>
#include <random>
#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include "ARCCache.h"
#include "FifoCache.h"
#include "LFUCache.h"
#include "LRUCache.h"


// MySQL数据库连接类
class MySQLDB {
private:
    MYSQL* connection;
    
public:
    MySQLDB() : connection(nullptr) {}
    
    bool connect(const std::string& host, const std::string& user, 
                 const std::string& password, const std::string& database, int port = 3306) {
        connection = mysql_init(nullptr);
        if (!connection) {
            std::cerr << "MySQL initialization failed" << std::endl;
            return false;
        }
        
        if (!mysql_real_connect(connection, host.c_str(), user.c_str(), 
                               password.c_str(), database.c_str(), port, nullptr, 0)) {
            std::cerr << "MySQL connection failed: " << mysql_error(connection) << std::endl;
            return false;
        }
        
        std::cout << "Connected to MySQL database successfully!" << std::endl;
        return true;
    }
    
    bool executeQuery(const std::string& query, std::vector<std::vector<std::string>>& results) {
        if (mysql_query(connection, query.c_str())) {
            std::cerr << "Query execution failed: " << mysql_error(connection) << std::endl;
            return false;
        }
        
        MYSQL_RES* result = mysql_store_result(connection);
        if (!result) {
            return true; // 查询成功但无结果(如INSERT, UPDATE等)
        }
        
        int num_fields = mysql_num_fields(result);
        MYSQL_ROW row;
        
        while ((row = mysql_fetch_row(result))) {
            std::vector<std::string> row_data;
            for (int i = 0; i < num_fields; i++) {
                row_data.push_back(row[i] ? row[i] : "NULL");
            }
            results.push_back(row_data);
        }
        
        mysql_free_result(result);
        return true;
    }
    
    // 模拟从数据库获取数据的方法
    bool getData(const std::string& key, std::string& value) {
        std::vector<std::vector<std::string>> results;
        std::string query = "SELECT cache_value FROM cache_test WHERE cache_key = '" + key + "'";
        
        if (executeQuery(query, results) && !results.empty()) {
            value = results[0][0];
            return true;
        }
        return false;
    }
    
    // 模拟向数据库插入数据的方法
    bool putData(const std::string& key, const std::string& value) {
        std::string query = "INSERT INTO cache_test (cache_key, cache_value) VALUES ('" + key + "', '" + value + "') "
                           "ON DUPLICATE KEY UPDATE cache_value = '" + value + "'";
        std::vector<std::vector<std::string>> results;
        return executeQuery(query, results);
    }
    
    ~MySQLDB() {
        if (connection) {
            mysql_close(connection);
        }
    }
};

// 缓存性能测试类
class CachePerformanceTest {
public:
    template<typename CacheType>
    static void testCache(CacheType& cache, const std::string& cache_name, 
                         const std::vector<std::pair<std::string, std::string>>& test_data,
                         int iterations) {
        std::cout << "\n=== Testing " << cache_name << " Cache ===" << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        int hits = 0, misses = 0;
        
        // 测试数据访问 - 创建更真实的访问模式
        std::random_device rd;
        std::mt19937 gen(rd());
        // 80% 的访问集中在20%的数据上(二八定律)
        std::uniform_int_distribution<> dis(0, test_data.size() - 1);
        std::discrete_distribution<> access_dist({80, 20}); // 80% 时间访问热数据
        
        // 定义热数据范围(前20%)
        size_t hot_data_size = test_data.size() / 5;
        
        for (int i = 0; i < iterations; i++) {
            int index;
            // 根据分布决定访问热数据还是冷数据
            if (access_dist(gen) == 0) {
                // 访问热数据(前20%)
                index = dis(gen) % hot_data_size;
            } else {
                // 访问冷数据(后80%)
                index = hot_data_size + (dis(gen) % (test_data.size() - hot_data_size));
            }
            
            // 替换结构化绑定为传统方式
            const std::string& key = test_data[index].first;
            const std::string& value = test_data[index].second;
            
            std::string retrieved_value;
            if (cache.get(key, retrieved_value)) {
                hits++;
            } else {
                cache.put(key, value);
                misses++;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double hit_rate = static_cast<double>(hits) / iterations * 100;
        
        std::cout << "Hits: " << hits << ", Misses: " << misses << std::endl;
        std::cout << "Hit Rate: " << std::fixed << std::setprecision(2) << hit_rate << "%" << std::endl;
        std::cout << "Time taken: " << duration.count() << " microseconds" << std::endl;
        std::cout << "Cache size: " << cache.size() << std::endl;
    }
    
    // 针对数据库访问的缓存测试
    template<typename CacheType>
    static void testDatabaseCache(CacheType& cache, const std::string& cache_name, MySQLDB& db,
                                 const std::vector<std::string>& test_keys, int iterations) {
        std::cout << "\n=== Testing " << cache_name << " Cache with Database Access ===" << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        int cache_hits = 0, db_hits = 0, db_misses = 0;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, test_keys.size() - 1);
        std::discrete_distribution<> access_dist({80, 20}); // 80% 时间访问热数据
        
        // 定义热数据范围(前20%)
        size_t hot_data_size = test_keys.size() / 5;
        
        for (int i = 0; i < iterations; i++) {
            int index;
            // 根据分布决定访问热数据还是冷数据
            if (access_dist(gen) == 0) {
                // 访问热数据(前20%)
                index = dis(gen) % hot_data_size;
            } else {
                // 访问冷数据(后80%)
                index = hot_data_size + (dis(gen) % (test_keys.size() - hot_data_size));
            }
            
            const std::string& key = test_keys[index];
            std::string value;
            
            // 首先尝试从缓存获取
            if (cache.get(key, value)) {
                cache_hits++;
            } else {
                // 缓存未命中，从数据库获取
                if (db.getData(key, value)) {
                    // 数据库中有数据
                    cache.put(key, value);
                    db_hits++;
                } else {
                    // 数据库中也没有数据，创建新数据
                    value = "value_for_" + key;
                    db.putData(key, value);
                    cache.put(key, value);
                    db_misses++;
                }
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        int total_requests = cache_hits + db_hits + db_misses;
        double cache_hit_rate = static_cast<double>(cache_hits) / total_requests * 100;
        double db_hit_rate = static_cast<double>(db_hits) / total_requests * 100;
        double db_miss_rate = static_cast<double>(db_misses) / total_requests * 100;
        
        std::cout << "Cache Hits: " << cache_hits << " (" << std::fixed << std::setprecision(2) << cache_hit_rate << "%)" << std::endl;
        std::cout << "Database Hits: " << db_hits << " (" << std::fixed << std::setprecision(2) << db_hit_rate << "%)" << std::endl;
        std::cout << "Database Misses: " << db_misses << " (" << std::fixed << std::setprecision(2) << db_miss_rate << "%)" << std::endl;
        std::cout << "Total Requests: " << total_requests << std::endl;
        std::cout << "Time taken: " << duration.count() << " microseconds" << std::endl;
        std::cout << "Cache size: " << cache.size() << std::endl;
    }
};

// 生成测试数据
std::vector<std::pair<std::string, std::string>> generateTestData(int count) {
    std::vector<std::pair<std::string, std::string>> data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 1000000);
    
    for (int i = 0; i < count; i++) {
        std::stringstream key, value;
        key << "key_" << dis(gen);
        value << "value_" << dis(gen);
        data.push_back(std::make_pair(key.str(), value.str()));
    }
    
    return data;
}

// 生成测试键列表
std::vector<std::string> generateTestKeys(int count) {
    std::vector<std::string> keys;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 1000000);
    
    for (int i = 0; i < count; i++) {
        std::stringstream key;
        key << "db_key_" << dis(gen);
        keys.push_back(key.str());
    }
    
    return keys;
}

int main() {
    std::cout << "Cache System Implementation with MySQL Integration" << std::endl;
    
    // 1. 测试各种缓存策略
    const size_t CACHE_SIZE = 100;
    const int TEST_ITERATIONS = 10000;
    
    // 创建各种缓存实例
    FIFOCache<std::string> fifo_cache(CACHE_SIZE);
    LRUCache<std::string> lru_cache(CACHE_SIZE);
    LFUCache<std::string> lfu_cache(CACHE_SIZE);
    ARCCache<std::string> arc_cache(CACHE_SIZE);
    
    // 生成测试数据
    std::cout << "Generating test data..." << std::endl;
    std::vector<std::pair<std::string, std::string>> test_data = generateTestData(1000);
    
    // 测试各种缓存策略
    CachePerformanceTest::testCache(fifo_cache, "FIFO", test_data, TEST_ITERATIONS);
    CachePerformanceTest::testCache(lru_cache, "LRU", test_data, TEST_ITERATIONS);
    CachePerformanceTest::testCache(lfu_cache, "LFU", test_data, TEST_ITERATIONS);
    CachePerformanceTest::testCache(arc_cache, "ARC", test_data, TEST_ITERATIONS);
    
    // 2. MySQL数据库连接测试
    std::cout << "\n=== MySQL Database Connection Test ===" << std::endl;
    MySQLDB db;
    
    if (db.connect("localhost", "ikun", "1234", "cache_test")) {
        std::cout << "MySQL connection test passed!" << std::endl;
        
        // 3. 测试数据库访问的缓存策略
        std::cout << "\n=== Database Cache Performance Test ===" << std::endl;
        std::vector<std::string> test_keys = generateTestKeys(1000);
        const int DB_TEST_ITERATIONS = 5000;
        
        // 为数据库测试重新创建缓存实例
        FIFOCache<std::string> db_fifo_cache(CACHE_SIZE);
        LRUCache<std::string> db_lru_cache(CACHE_SIZE);
        LFUCache<std::string> db_lfu_cache(CACHE_SIZE);
        ARCCache<std::string> db_arc_cache(CACHE_SIZE);
        
        // 测试各种缓存策略在数据库访问场景下的性能
        CachePerformanceTest::testDatabaseCache(db_fifo_cache, "FIFO", db, test_keys, DB_TEST_ITERATIONS);
        CachePerformanceTest::testDatabaseCache(db_lru_cache, "LRU", db, test_keys, DB_TEST_ITERATIONS);
        CachePerformanceTest::testDatabaseCache(db_lfu_cache, "LFU", db, test_keys, DB_TEST_ITERATIONS);
        CachePerformanceTest::testDatabaseCache(db_arc_cache, "ARC", db, test_keys, DB_TEST_ITERATIONS);
    } else {
        std::cout << "MySQL connection test failed. Please check your MySQL configuration." << std::endl;
    }
    
    std::cout << "\nCache system testing completed!" << std::endl;
    
    return 0;
}