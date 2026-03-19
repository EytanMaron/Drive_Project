/**
 * FileSystemStorage Thread-Safety Unit Tests
 * 
 * These tests are written before adding mutex protection to FileSystemStorage.
 * They are designed to aggressively expose race conditions and thread-safety issues.
 * 
 * Testing Strategy:
 * - Uses std::thread to create multiple threads
 * - Tests are designed to maximize race condition probability
 * - No delays between operations to maximize contention
 * - Tests will fail consistently without mutex protection
 */

#include <gtest/gtest.h>
#include "FileSystemStorage.h"
#include <thread>
#include <vector>
#include <string>
#include <filesystem>
#include <atomic>
#include <algorithm>
#include <set>
#include <mutex>
#include <condition_variable>

// Helper class to coordinate thread start - maximizes race condition probability
class ThreadBarrier {
public:
    explicit ThreadBarrier(size_t count) : m_count(count), m_waiting(0) {}
    
    void wait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_waiting++;
        if (m_waiting == m_count) {
            m_cv.notify_all();
        } else {
            m_cv.wait(lock, [this] { return m_waiting == m_count; });
        }
    }
    
private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    size_t m_count;
    size_t m_waiting;
};

// Aggressive concurrent writes to same file - should cause corruption without mutex
// This test writes and immediately reads back, maximizing chance of reading partial or corrupted data
TEST(FileSystemStorageThreadSafety, ConcurrentWriteReadRace) {
    FileSystemStorage storage;
    const std::string testFile = "race_test_file.txt";
    const int numThreads = 10;
    const int operationsPerThread = 50;
    
    std::filesystem::remove(testFile);
    
    std::vector<std::thread> threads;
    std::atomic<int> corruptionDetected(0);
    std::atomic<int> totalOperations(0);
    ThreadBarrier barrier(numThreads);
    
    // all threads write and immediately read the same file
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            barrier.wait();  // all threads start simultaneously
            
            for (int i = 0; i < operationsPerThread; ++i) {
                // create distinct content for each thread and operation
                std::string writeContent = "Thread" + std::to_string(t) + "_Op" + std::to_string(i) + "_" + std::string(500, 'X' + t);
                
                try {
                    storage.write(testFile, writeContent);
                    totalOperations++;
                    
                    // immediately read back - no delay to maximize race condition
                    std::string readContent = storage.read(testFile);
                    
                    // Verify integrity: content must be exactly what we wrote OR a complete write from another thread
                    bool isValid = (readContent == writeContent);
                    
                    if (!isValid) {
                        // Check if it's a valid complete write from another thread
                        bool isCompleteOtherWrite = false;
                        for (int other = 0; other < numThreads; ++other) {
                            std::string expectedPrefix = "Thread" + std::to_string(other) + "_Op";
                            if (readContent.find(expectedPrefix) == 0) {
                                // Verify it's complete 
                                size_t expectedLength = expectedPrefix.length() + 3 + 1 + 500; 
                                if (readContent.length() >= expectedLength - 10 && 
                                    readContent.length() <= expectedLength + 10) {
                                    isCompleteOtherWrite = true;
                                    break;
                                }
                            }
                        }
                        
                        if (!isCompleteOtherWrite) {
                            // ++ for partial write, mixed content, or invalid data
                            corruptionDetected++;
                        }
                    }
                } catch (...) {
                    totalOperations++;
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    
    EXPECT_EQ(corruptionDetected.load(), 0) 
        << "Race condition detected: " << corruptionDetected.load() 
        << " corrupted reads out of " << totalOperations.load() << " operations. "
        << "This proves FileSystemStorage needs mutex protection.";
    
    std::filesystem::remove(testFile);
}

