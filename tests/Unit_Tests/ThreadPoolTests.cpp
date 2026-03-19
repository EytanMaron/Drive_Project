/**
 * @file ThreadPoolTests.cpp
 * @brief Unit tests for ThreadPool class.
 * 
 * Tests cover:
 * - Construction with valid/invalid parameters
 * - Task execution (single and multiple)
 * - Concurrent enqueueing from multiple threads
 * - Graceful shutdown behavior
 * - Exception handling in tasks
 */

#include <gtest/gtest.h>
#include "Server/ThreadPool.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>

// Test fixture for ThreadPool tests
class ThreadPoolTests : public ::testing::Test {
protected:
    // Small pool for faster tests
    static constexpr size_t TEST_POOL_SIZE = 4;
};

// CONSTRUCTION TESTS

TEST_F(ThreadPoolTests, ConstructWithValidSize_CreatesPool) {
    // Arrange & Act
    ThreadPool pool(TEST_POOL_SIZE);
    
    // Assert
    EXPECT_EQ(pool.size(), TEST_POOL_SIZE);
    EXPECT_TRUE(pool.isRunning());
}

TEST_F(ThreadPoolTests, ConstructWithZeroSize_ThrowsInvalidArgument) {
    // Arrange, Act & Assert
    EXPECT_THROW(ThreadPool pool(0), std::invalid_argument);
}

TEST_F(ThreadPoolTests, DefaultConstruction_UsesDefaultSize) {
    // Arrange & Act
    ThreadPool pool;
    
    // Assert - default is 20 threads
    EXPECT_EQ(pool.size(), 20);
    EXPECT_TRUE(pool.isRunning());
}

// SINGLE TASK EXECUTION TESTS

TEST_F(ThreadPoolTests, EnqueueSingleTask_TaskExecutes) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    std::atomic<bool> taskExecuted{false};
    
    // Act
    pool.enqueue([&taskExecuted]() {
        taskExecuted = true;
    });
    
    // Wait for task to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Assert
    EXPECT_TRUE(taskExecuted);
}

TEST_F(ThreadPoolTests, EnqueueEmptyTask_ThrowsInvalidArgument) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    Task emptyTask;
    
    // Act & Assert
    EXPECT_THROW(pool.enqueue(emptyTask), std::invalid_argument);
}

// MULTIPLE TASK EXECUTION TESTS

TEST_F(ThreadPoolTests, EnqueueMultipleTasks_AllTasksExecute) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    const int numTasks = 10;
    std::atomic<int> counter{0};
    
    // Act
    for (int i = 0; i < numTasks; ++i) {
        pool.enqueue([&counter]() {
            counter++;
        });
    }
    
    // Wait for all tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Assert
    EXPECT_EQ(counter.load(), numTasks);
}

TEST_F(ThreadPoolTests, TasksExecuteConcurrently_MultipleWorkersActive) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    std::atomic<int> activeWorkers{0};
    std::atomic<int> maxConcurrent{0};
    std::mutex mutex;
    
    // Act - enqueue tasks that track concurrent execution
    for (size_t i = 0; i < TEST_POOL_SIZE * 2; ++i) {
        pool.enqueue([&activeWorkers, &maxConcurrent]() {
            int current = ++activeWorkers;
            
            // Update max concurrent if this is a new high
            int expected = maxConcurrent.load();
            while (current > expected && 
                   !maxConcurrent.compare_exchange_weak(expected, current)) {
                // Retry CAS
            }
            
            // Simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            --activeWorkers;
        });
    }
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Assert - multiple workers should have been active concurrently
    EXPECT_GT(maxConcurrent.load(), 1);
}

// CONCURRENT ENQUEUEING TESTS

TEST_F(ThreadPoolTests, ConcurrentEnqueue_NoRaceConditions) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    const int numProducers = 4;
    const int tasksPerProducer = 25;
    std::atomic<int> counter{0};
    
    // Act - multiple threads enqueueing simultaneously
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&pool, &counter, tasksPerProducer]() {
            for (int t = 0; t < tasksPerProducer; ++t) {
                pool.enqueue([&counter]() {
                    counter++;
                });
            }
        });
    }
    
    // Wait for producers to finish enqueueing
    for (auto& producer : producers) {
        producer.join();
    }
    
    // Wait for all tasks to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Assert
    EXPECT_EQ(counter.load(), numProducers * tasksPerProducer);
}

// SHUTDOWN TESTS

TEST_F(ThreadPoolTests, ShutdownWithPendingTasks_CompletesAll) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    std::atomic<int> counter{0};
    
    // Enqueue tasks that take some time
    for (int i = 0; i < 10; ++i) {
        pool.enqueue([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            counter++;
        });
    }
    
    // Act - shutdown while tasks are pending
    pool.shutdown();
    
    // Assert - all tasks should have completed
    EXPECT_EQ(counter.load(), 10);
}

TEST_F(ThreadPoolTests, ShutdownIsIdempotent_MultipleCallsSafe) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    
    // Act - multiple shutdown calls
    pool.shutdown();
    pool.shutdown();
    pool.shutdown();
    
    // Assert - no crash, pool is stopped
    EXPECT_FALSE(pool.isRunning());
}

TEST_F(ThreadPoolTests, EnqueueAfterShutdown_ThrowsRuntimeError) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    pool.shutdown();
    
    // Act & Assert
    EXPECT_THROW(pool.enqueue([]() {}), std::runtime_error);
}

TEST_F(ThreadPoolTests, IsRunningReturnsFalseAfterShutdown) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    EXPECT_TRUE(pool.isRunning());
    
    // Act
    pool.shutdown();
    
    // Assert
    EXPECT_FALSE(pool.isRunning());
}

// EXCEPTION HANDLING TESTS

TEST_F(ThreadPoolTests, TaskThrowsException_WorkerContinues) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    std::atomic<int> successCount{0};
    
    // Act - enqueue mix of throwing and normal tasks
    pool.enqueue([]() {
        throw std::runtime_error("Task failure");
    });
    
    // Enqueue more tasks after the throwing one
    for (int i = 0; i < 5; ++i) {
        pool.enqueue([&successCount]() {
            successCount++;
        });
    }
    
    // Wait for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Assert - other tasks should still execute
    EXPECT_EQ(successCount.load(), 5);
}

// DESTRUCTOR TESTS

TEST_F(ThreadPoolTests, DestructorCallsShutdown_GracefulCleanup) {
    // Arrange
    std::atomic<int> counter{0};
    
    {
        ThreadPool pool(TEST_POOL_SIZE);
        
        for (int i = 0; i < 5; ++i) {
            pool.enqueue([&counter]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                counter++;
            });
        }
        
        // Pool goes out of scope here - destructor should wait for tasks
    }
    
    // Assert - all tasks should have completed before destructor returned
    EXPECT_EQ(counter.load(), 5);
}

TEST_F(ThreadPoolTests, ShutdownFromWorker_DoesNotDeadlock) {
    // Arrange
    ThreadPool pool(TEST_POOL_SIZE);
    
    // Act
    pool.enqueue([&pool]() {
        // Call shutdown from within the pool
        pool.shutdown();
    });
    
    // Wait for shutdown to complete (should be near instantaneous logic-wise)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Assert
    EXPECT_FALSE(pool.isRunning());
}
