cmake_minimum_required(VERSION 3.10)
project(ThreadPool VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set vcpkg toolchain file if it exists
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

# Find GTest package
find_package(GTest REQUIRED)

# Add include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add source files
set(SOURCES
    src/thread_pool.cpp
)

# Create main library
add_library(thread_pool STATIC ${SOURCES})

# Add test executable
enable_testing()
add_executable(thread_pool_test tests/thread_pool_test.cpp)
target_link_libraries(thread_pool_test PRIVATE thread_pool GTest::GTest GTest::Main)

# Add example executable
add_executable(thread_pool_example examples/main.cpp)
target_link_libraries(thread_pool_example PRIVATE thread_pool)

# Set compiler flags
if(MSVC)
    target_compile_options(thread_pool PRIVATE /W4)
else()
    target_compile_options(thread_pool PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install targets
install(TARGETS thread_pool
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include) 