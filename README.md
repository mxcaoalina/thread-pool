# Thread Pool Optimization System

A high-performance, feature-rich thread pool implementation in C++ that includes adaptive scaling, deadlock detection, and performance monitoring.

## Features

- **Priority-based Task Scheduling**: Tasks are executed based on priority levels
- **Adaptive Thread Scaling**: Automatically adjusts the number of worker threads based on workload
- **Deadlock Detection**: Implements resource allocation graphs to detect potential deadlocks
- **Performance Monitoring**: Tracks throughput and system metrics
- **Thread-Safe Design**: Implements proper synchronization using mutexes and condition variables
- **Resource Management**: Efficient handling of system resources

## Building the Project

### Prerequisites

- C++17 or later
- CMake 3.10 or later
- A C++ compiler with thread support (GCC, Clang, or MSVC)

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Usage Example

```cpp
#include "thread_pool.hpp"

int main() {
    // Create a thread pool with 2 initial threads and up to 5 max threads
    ThreadPool pool(2, 5);

    // Enqueue tasks with different priorities
    for (int i = 0; i < 10; ++i) {
        pool.enqueue(Task{
            [i]() {
                // Your task implementation here
                std::cout << "Task " << i << " executed.\n";
            },
            i % 3  // priority = 0,1,2 cycling
        });
    }

    // Monitor performance
    std::cout << "Current throughput: " << pool.getThroughput() << " tasks/sec\n";

    // Clean shutdown
    pool.shutdown();
    return 0;
}
```

## Project Structure

```
.
├── CMakeLists.txt
├── README.md
├── include/
│   └── thread_pool.hpp
├── src/
│   ├── thread_pool.cpp
│   └── performance_monitor.cpp
└── tests/
    └── thread_pool_test.cpp
```

## Performance Monitoring

The system includes a built-in performance monitoring module that tracks:
- Tasks processed per second
- Queue size metrics
- Thread utilization
- Resource allocation patterns

## Deadlock Detection

The thread pool implements a resource allocation graph algorithm to detect potential deadlocks. It tracks:
- Resources held by each task
- Resources requested by each task
- Circular dependencies in resource allocation

## License

MIT License - feel free to use this code in your own projects.

## Author

Mingxi Cao