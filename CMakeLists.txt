cmake_minimum_required(VERSION 3.14)
project(feline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address -fsanitize=undefined")

add_executable(feline
    src/main.cpp
    src/graph.cpp
    src/index.cpp
    src/query.cpp
)
target_include_directories(feline PRIVATE include)

# Tools
add_executable(convert_adjlist tools/convert_adjlist.cpp)

enable_testing()
add_executable(feline_tests
    test/test_main.cpp
    src/graph.cpp
    src/index.cpp
    src/query.cpp
)
target_include_directories(feline_tests PRIVATE include)
add_test(NAME feline_tests COMMAND feline_tests)
