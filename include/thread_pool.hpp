#pragma once

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <functional>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace thread_pool {

// ---------------------------------------------------------------
// A simple "Task" structure that holds:
//   - A callable (function) to be executed
//   - A priority value
// ---------------------------------------------------------------
struct Task {
    std::function<void()> func;
    int priority;

    // For the priority queue to order tasks (highest priority first)
    bool operator<(const Task& other) const {
        return priority < other.priority;
    }
};

// ---------------------------------------------------------------
// A basic ResourceAllocationGraph to demonstrate deadlock detection
// ---------------------------------------------------------------
class ResourceAllocationGraph {
public:
    void addResource(int resourceID);
    void removeResource(int resourceID);
    void taskHoldsResource(int taskID, int resourceID);
    void taskRequestsResource(int taskID, int resourceID);
    void taskReleasesResource(int taskID, int resourceID);
    bool detectDeadlock();

private:
    std::mutex mtx_;
    std::unordered_set<int> resources_;
    std::unordered_map<int, std::unordered_set<int>> resourcesHeld_;
    std::unordered_map<int, std::unordered_set<int>> resourcesRequested_;
};

// ---------------------------------------------------------------
// A basic performance monitor that tracks tasks processed over time
// ---------------------------------------------------------------
class PerformanceMonitor {
public:
    PerformanceMonitor();
    void recordTaskCompletion();
    double getThroughput();
    void reset();

private:
    std::mutex mtx_;
    std::atomic<size_t> taskCount_;
    std::chrono::time_point<std::chrono::steady_clock> start_;
};

// ---------------------------------------------------------------
// ThreadPool with adaptive thread scaling & priority scheduling
// ---------------------------------------------------------------
class ThreadPool {
public:
    explicit ThreadPool(size_t minThreads, size_t maxThreads);
    ~ThreadPool();

    // Task management
    void enqueue(const Task& task);
    void shutdown();

    // Resource management
    void requestResource(int taskID, int resourceID);
    void holdResource(int taskID, int resourceID);
    void releaseResource(int taskID, int resourceID);
    bool checkDeadlock();

    // Performance monitoring
    double getThroughput();

private:
    void workerThread();
    void maybeScaleUp();
    void maybeScaleDown();

private:
    std::vector<std::thread> workers_;
    std::priority_queue<Task> taskQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCond_;
    std::atomic<bool> done_;

    size_t minThreads_;
    size_t maxThreads_;

    ResourceAllocationGraph rag_;
    PerformanceMonitor perfMonitor_;
};

} // namespace thread_pool 