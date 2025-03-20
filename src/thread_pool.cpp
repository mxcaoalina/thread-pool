#include "thread_pool.hpp"
#include <iostream>

namespace thread_pool {

// ResourceAllocationGraph implementation
void ResourceAllocationGraph::addResource(int resourceID) {
    std::lock_guard<std::mutex> lock(mtx_);
    resources_.insert(resourceID);
}

void ResourceAllocationGraph::removeResource(int resourceID) {
    std::lock_guard<std::mutex> lock(mtx_);
    resources_.erase(resourceID);
    for (auto& [taskID, held] : resourcesHeld_) {
        held.erase(resourceID);
    }
    for (auto& [taskID, requested] : resourcesRequested_) {
        requested.erase(resourceID);
    }
}

void ResourceAllocationGraph::taskHoldsResource(int taskID, int resourceID) {
    std::lock_guard<std::mutex> lock(mtx_);
    resourcesHeld_[taskID].insert(resourceID);
    resourcesRequested_[taskID].erase(resourceID);
}

void ResourceAllocationGraph::taskRequestsResource(int taskID, int resourceID) {
    std::lock_guard<std::mutex> lock(mtx_);
    resourcesRequested_[taskID].insert(resourceID);
}

void ResourceAllocationGraph::taskReleasesResource(int taskID, int resourceID) {
    std::lock_guard<std::mutex> lock(mtx_);
    resourcesHeld_[taskID].erase(resourceID);
    resourcesRequested_[taskID].erase(resourceID);
}

bool ResourceAllocationGraph::detectDeadlock() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::unordered_map<int, std::vector<int>> graph;
    std::unordered_set<int> visited;
    std::unordered_set<int> recursionStack;

    // Build edges for tasks requesting resources
    for (auto& [taskID, reqSet] : resourcesRequested_) {
        for (auto& r : reqSet) {
            graph[-taskID].push_back(r);
        }
    }

    // Build edges for resources held by tasks
    for (auto& [taskID, heldSet] : resourcesHeld_) {
        for (auto& r : heldSet) {
            graph[r].push_back(-taskID);
        }
    }

    std::function<bool(int)> dfsCycleCheck = [&](int node) -> bool {
        if (!visited.count(node)) {
            visited.insert(node);
            recursionStack.insert(node);

            if (graph.find(node) != graph.end()) {
                for (int neighbor : graph[node]) {
                    if (!visited.count(neighbor) && dfsCycleCheck(neighbor)) {
                        return true;
                    } else if (recursionStack.count(neighbor)) {
                        return true;
                    }
                }
            }
        }
        recursionStack.erase(node);
        return false;
    };

    for (auto& pair : graph) {
        if (!visited.count(pair.first) && dfsCycleCheck(pair.first)) {
            return true;
        }
    }

    return false;
}

// PerformanceMonitor implementation
PerformanceMonitor::PerformanceMonitor() 
    : taskCount_(0), start_(std::chrono::steady_clock::now()) {}

void PerformanceMonitor::recordTaskCompletion() {
    std::lock_guard<std::mutex> lock(mtx_);
    taskCount_++;
}

double PerformanceMonitor::getThroughput() {
    std::lock_guard<std::mutex> lock(mtx_);
    auto now = std::chrono::steady_clock::now();
    double secondsElapsed = std::chrono::duration<double>(now - start_).count();
    if (secondsElapsed == 0) return 0.0;
    return static_cast<double>(taskCount_) / secondsElapsed;
}

void PerformanceMonitor::reset() {
    std::lock_guard<std::mutex> lock(mtx_);
    taskCount_ = 0;
    start_ = std::chrono::steady_clock::now();
}

// ThreadPool implementation
ThreadPool::ThreadPool(size_t minThreads, size_t maxThreads)
    : minThreads_(minThreads),
      maxThreads_(maxThreads),
      done_(false) {
    for (size_t i = 0; i < minThreads_; i++) {
        workers_.emplace_back(std::thread(&ThreadPool::workerThread, this));
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::enqueue(const Task& task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(task);
    }
    queueCond_.notify_one();
    maybeScaleUp();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        done_ = true;
    }
    queueCond_.notify_all();

    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
    workers_.clear();
}

void ThreadPool::requestResource(int taskID, int resourceID) {
    rag_.taskRequestsResource(taskID, resourceID);
}

void ThreadPool::holdResource(int taskID, int resourceID) {
    rag_.taskHoldsResource(taskID, resourceID);
}

void ThreadPool::releaseResource(int taskID, int resourceID) {
    rag_.taskReleasesResource(taskID, resourceID);
}

bool ThreadPool::checkDeadlock() {
    return rag_.detectDeadlock();
}

double ThreadPool::getThroughput() {
    return perfMonitor_.getThroughput();
}

void ThreadPool::workerThread() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCond_.wait(lock, [this]() {
                return !taskQueue_.empty() || done_;
            });

            if (done_ && taskQueue_.empty()) {
                return;
            }
            
            task = taskQueue_.top();
            taskQueue_.pop();
        }

        task.func();
        perfMonitor_.recordTaskCompletion();
        maybeScaleDown();
    }
}

void ThreadPool::maybeScaleUp() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (taskQueue_.size() > workers_.size() && workers_.size() < maxThreads_) {
        workers_.emplace_back(std::thread(&ThreadPool::workerThread, this));
    }
}

void ThreadPool::maybeScaleDown() {
    // This is a simplified version. In a real system, you would need
    // more sophisticated logic to safely scale down threads.
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (taskQueue_.size() < workers_.size() / 2 && workers_.size() > minThreads_) {
        // In a real implementation, you would need to coordinate
        // with the worker threads to safely remove one.
        // This is just a placeholder for the concept.
    }
}

} // namespace thread_pool 