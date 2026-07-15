cmake_minimum_required(VERSION 3.14)
project(feline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address -fsanitize=undefined")

option(FELINE_BUILD_DYNAMIC "Build the dynamic (Feline-PK) subproject" OFF)

# Shared core library (static + dynamic both depend on this)
add_library(feline_core STATIC
    src/graph.cpp
    src/index.cpp
    src/query.cpp
)
target_include_directories(feline_core PUBLIC include)

# Static CLI
add_executable(feline src/main.cpp)
target_link_libraries(feline PRIVATE feline_core)

# Tools
add_executable(convert_adjlist tools/convert_adjlist.cpp)

enable_testing()

# Static tests
add_executable(feline_tests test/test_main.cpp)
target_link_libraries(feline_tests PRIVATE feline_core)
add_test(NAME feline_tests COMMAND feline_tests)

if(FELINE_BUILD_DYNAMIC)
    add_subdirectory(dynamic)
endif()
