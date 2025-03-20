#include "thread_pool.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>

using namespace thread_pool;

// Test fixture for thread pool tests
class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool = std::make_unique<ThreadPool>(2, 4);
    }

    void TearDown() override {
        pool->shutdown();
    }

    std::unique_ptr<ThreadPool> pool;
};

// Test basic task execution
TEST_F(ThreadPoolTest, BasicTaskExecution) {
    std::atomic<int> counter{0};
    
    // Enqueue 5 tasks
    for (int i = 0; i < 5; ++i) {
        pool->enqueue(Task{
            [&counter]() { counter++; },
            0
        });
    }

    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(counter, 5);
}

// Test priority scheduling
TEST_F(ThreadPoolTest, PriorityScheduling) {
    std::vector<int> execution_order;
    std::mutex order_mutex;

    // Enqueue tasks with different priorities
    for (int i = 0; i < 3; ++i) {
        pool->enqueue(Task{
            [&execution_order, &order_mutex, i]() {
                std::lock_guard<std::mutex> lock(order_mutex);
                execution_order.push_back(i);
            },
            2 - i  // priorities: 2, 1, 0
        });
    }

    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Higher priority tasks should execute first
    EXPECT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[0], 0);  // priority 2
    EXPECT_EQ(execution_order[1], 1);  // priority 1
    EXPECT_EQ(execution_order[2], 2);  // priority 0
}

// Test deadlock detection
TEST_F(ThreadPoolTest, DeadlockDetection) {
    // Create a deadlock scenario
    pool->requestResource(1, 100);
    pool->holdResource(1, 100);
    pool->requestResource(2, 200);
    pool->holdResource(2, 200);
    
    // Request resources in a way that creates a cycle
    pool->requestResource(1, 200);  // Task 1 requests resource held by Task 2
    pool->requestResource(2, 100);  // Task 2 requests resource held by Task 1

    EXPECT_TRUE(pool->checkDeadlock());

    // Clean up
    pool->releaseResource(1, 100);
    pool->releaseResource(2, 200);
}

// Test performance monitoring
TEST_F(ThreadPoolTest, PerformanceMonitoring) {
    // Enqueue some tasks
    for (int i = 0; i < 5; ++i) {
        pool->enqueue(Task{
            []() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); },
            0
        });
    }

    // Wait for tasks to start processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check throughput
    double throughput = pool->getThroughput();
    EXPECT_GT(throughput, 0.0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 