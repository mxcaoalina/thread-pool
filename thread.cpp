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
// A basic ResourceAllocationGraph to demonstrate deadlock detection:
//   - We maintain a mapping of "taskID -> resourcesHeld"
//   - Another mapping of "taskID -> resourcesRequested"
//   - We can attempt to detect cycles by building a graph of
//     Resource -> Task -> Resource -> ...
// ---------------------------------------------------------------
class ResourceAllocationGraph {
public:
    // "Resource" is just an integer ID in this simple example
    void addResource(int resourceID) {
        std::lock_guard<std::mutex> lock(mtx_);
        resources_.insert(resourceID);
    }

    void removeResource(int resourceID) {
        std::lock_guard<std::mutex> lock(mtx_);
        resources_.erase(resourceID);
        // Also remove any references from tasks
        for (auto& [taskID, held] : resourcesHeld_) {
            held.erase(resourceID);
        }
        for (auto& [taskID, requested] : resourcesRequested_) {
            requested.erase(resourceID);
        }
    }

    void taskHoldsResource(int taskID, int resourceID) {
        std::lock_guard<std::mutex> lock(mtx_);
        resourcesHeld_[taskID].insert(resourceID);
        // If we hold a resource, we no longer "request" it
        resourcesRequested_[taskID].erase(resourceID);
    }

    void taskRequestsResource(int taskID, int resourceID) {
        std::lock_guard<std::mutex> lock(mtx_);
        resourcesRequested_[taskID].insert(resourceID);
    }

    void taskReleasesResource(int taskID, int resourceID) {
        std::lock_guard<std::mutex> lock(mtx_);
        resourcesHeld_[taskID].erase(resourceID);
        resourcesRequested_[taskID].erase(resourceID);
    }

    bool detectDeadlock() {
        std::lock_guard<std::mutex> lock(mtx_);
        // Build a graph: For each task T that requests resource R,
        // connect T -> R. For each task T that holds resource R,
        // connect R -> T. If there's a cycle, we suspect deadlock.

        // We'll represent tasks by negative IDs and resources by positive IDs
        // (just a simple trick to unify them in one adjacency structure).
        // Alternatively, we can keep them separate.

        // Adjacency list
        std::unordered_map<int, std::vector<int>> graph;

        // Build edges for tasks that are requesting resources
        for (auto& [taskID, reqSet] : resourcesRequested_) {
            for (auto& r : reqSet) {
                graph[-taskID].push_back(r);  // T -> R
            }
        }

        // Build edges for resources that are held by tasks
        for (auto& [taskID, heldSet] : resourcesHeld_) {
            for (auto& r : heldSet) {
                graph[r].push_back(-taskID);  // R -> T
            }
        }

        // We can now do a DFS to find a cycle.
        std::unordered_set<int> visited;
        std::unordered_set<int> recursionStack; // tracks nodes in the current path

        // Lambda for DFS
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

        // Run DFS from each unvisited node
        for (auto& pair : graph) {
            int node = pair.first;
            if (!visited.count(node)) {
                if (dfsCycleCheck(node)) {
                    return true; // cycle found
                }
            }
        }

        return false;
    }

private:
    std::mutex mtx_;
    std::unordered_set<int> resources_;
    std::unordered_map<int, std::unordered_set<int>> resourcesHeld_;
    std::unordered_map<int, std::unordered_set<int>> resourcesRequested_;
};

// ---------------------------------------------------------------
// A basic performance monitor that tracks tasks processed over time
// and can measure throughput (tasks/second).
// ---------------------------------------------------------------
class PerformanceMonitor {
public:
    PerformanceMonitor() : taskCount_(0), start_(std::chrono::steady_clock::now()) {}

    void recordTaskCompletion() {
        std::lock_guard<std::mutex> lock(mtx_);
        taskCount_++;
    }

    double getThroughput() {
        std::lock_guard<std::mutex> lock(mtx_);
        auto now = std::chrono::steady_clock::now();
        double secondsElapsed = std::chrono::duration<double>(now - start_).count();
        if (secondsElapsed == 0) return 0.0;
        return static_cast<double>(taskCount_) / secondsElapsed;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mtx_);
        taskCount_ = 0;
        start_ = std::chrono::steady_clock::now();
    }

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
    ThreadPool(size_t minThreads, size_t maxThreads)
        : minThreads_(minThreads),
          maxThreads_(maxThreads),
          done_(false) {

        // Initially start with minThreads
        for (size_t i = 0; i < minThreads_; i++) {
            workers_.emplace_back(std::thread(&ThreadPool::workerThread, this));
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    // Enqueue a task with a certain priority
    void enqueue(const Task& task) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            taskQueue_.push(task);
        }
        queueCond_.notify_one();

        maybeScaleUp();
    }

    // Force a resource request event for demonstration
    void requestResource(int taskID, int resourceID) {
        rag_.taskRequestsResource(taskID, resourceID);
    }

    // Force a resource hold event for demonstration
    void holdResource(int taskID, int resourceID) {
        rag_.taskHoldsResource(taskID, resourceID);
    }

    // Force a resource release event
    void releaseResource(int taskID, int resourceID) {
        rag_.taskReleasesResource(taskID, resourceID);
    }

    // Check for deadlock
    bool checkDeadlock() {
        return rag_.detectDeadlock();
    }

    // Shutdown the thread pool
    void shutdown() {
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

    // Performance monitor accessor
    double getThroughput() {
        return perfMonitor_.getThroughput();
    }

private:
    void workerThread() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                // Wait until there's a task or the pool is done
                queueCond_.wait(lock, [this]() {
                    return !taskQueue_.empty() || done_;
                });

                if (done_ && taskQueue_.empty()) {
                    return;
                }
                
                // Pop the highest-priority task
                task = taskQueue_.top();
                taskQueue_.pop();
            }

            // Execute the task
            task.func();
            perfMonitor_.recordTaskCompletion();

            // Possibly scale down
            maybeScaleDown();
        }
    }

    // Adaptive scaling logic: if the queue is large,
    // and we haven't hit maxThreads_, add a new thread
    void maybeScaleUp() {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (taskQueue_.size() > workers_.size() && workers_.size() < maxThreads_) {
            workers_.emplace_back(std::thread(&ThreadPool::workerThread, this));
        }
    }

    // If we have significantly fewer tasks than threads,
    // we might allow a thread to exit (in a real system, we’d do this carefully).
    void maybeScaleDown() {
        // This naive approach checks if we have more threads than tasks
        // and we exceed minThreads_. Then we can let this thread exit
        // (by setting done_ = true for *one* thread) in a real system,
        // or do something more graceful.

        // For demonstration, we won't actually kill a worker in the middle of run
        // because that's quite involved. We’ll just illustrate the concept.

        // Pseudocode demonstration:
        // if (taskQueue_.size() < workers_.size() / 2 && workers_.size() > minThreads_) {
        //     std::thread::id thisId = std::this_thread::get_id();
        //     // Possibly remove a thread from the pool, etc.
        // }
    }

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

// ---------------------------------------------------------------
// Example usage
// ---------------------------------------------------------------
int main() {
    // Create a thread pool with 2 initial threads and up to 5 max threads
    ThreadPool pool(2, 5);

    // Suppose we have 10 tasks of varying priority
    for (int i = 0; i < 10; ++i) {
        pool.enqueue(Task{
            [i]() {
                // Simulate some work
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                std::cout << "Task " << i << " executed.\n";
            },
            i % 3 // priority = 0,1,2 cycling
        });
    }

    // Demonstrate resource usage to show how you might check deadlock
    // For a real deadlock, tasks must request resources in a circular manner.
    // This snippet doesn’t create a real deadlock, but illustrates usage:
    pool.requestResource(1, 100); // Task 1 requests resource 100
    pool.holdResource(1, 100);    // Task 1 holds resource 100

    pool.requestResource(2, 200); // Task 2 requests resource 200
    pool.holdResource(2, 200);    // Task 2 holds resource 200

    // Attempt to detect deadlock (should be false here)
    bool deadlocked = pool.checkDeadlock();
    std::cout << "Deadlock detected? " << std::boolalpha << deadlocked << "\n";

    // Wait a bit for tasks to finish
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check throughput
    std::cout << "Current throughput: " << pool.getThroughput() << " tasks/sec\n";

    // Gracefully shut down
    pool.shutdown();
    return 0;
}
