#include "thread_pool.hpp"
#include <iostream>
#include <chrono>
#include <random>

using namespace thread_pool;

// Helper function to simulate work
void simulateWork(int taskId, int duration_ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    std::cout << "Task " << taskId << " completed in " << duration_ms << "ms\n";
}

int main() {
    // Create a thread pool with 2 initial threads and up to 5 max threads
    ThreadPool pool(2, 5);

    // Add some resources to the system
    pool.requestResource(1, 100);  // Task 1 requests resource 100
    pool.holdResource(1, 100);     // Task 1 holds resource 100

    pool.requestResource(2, 200);  // Task 2 requests resource 200
    pool.holdResource(2, 200);     // Task 2 holds resource 200

    // Create a random number generator for task durations
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(50, 200);

    // Enqueue 10 tasks with different priorities
    for (int i = 0; i < 10; ++i) {
        pool.enqueue(Task{
            [i, &dis, &gen]() {
                simulateWork(i, dis(gen));
            },
            i % 3  // priority = 0,1,2 cycling
        });
    }

    // Demonstrate deadlock detection
    bool deadlocked = pool.checkDeadlock();
    std::cout << "Deadlock detected? " << std::boolalpha << deadlocked << "\n";

    // Wait a bit for tasks to start processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check throughput
    std::cout << "Current throughput: " << pool.getThroughput() << " tasks/sec\n";

    // Release resources
    pool.releaseResource(1, 100);
    pool.releaseResource(2, 200);

    // Wait for all tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check final throughput
    std::cout << "Final throughput: " << pool.getThroughput() << " tasks/sec\n";

    // Gracefully shut down
    pool.shutdown();
    return 0;
} 