# Dynamic (Feline-PK) subproject. Built only when FELINE_BUILD_DYNAMIC=ON.

add_library(feline_dyn STATIC
    src/dyn_graph.cpp
    src/dyn_index.cpp
    src/dyn_query.cpp
    src/dyn_scc.cpp
    src/feline_pk.cpp
)
target_include_directories(feline_dyn PUBLIC include)
target_link_libraries(feline_dyn PUBLIC feline_core)

add_executable(feline_dyn_tests test/test_dynamic.cpp)
target_link_libraries(feline_dyn_tests PRIVATE feline_dyn)
add_test(NAME feline_dyn_tests COMMAND feline_dyn_tests
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(dyn_demo tools/dyn_demo.cpp)
target_link_libraries(dyn_demo PRIVATE feline_dyn)

add_executable(dyn_incremental tools/dyn_incremental.cpp)
target_link_libraries(dyn_incremental PRIVATE feline_dyn)
