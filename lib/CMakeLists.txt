cmake_minimum_required(VERSION 3.9.1)
project(SpaceControl)


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pthread -fopenmp -g")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/)

# Create the library target
add_library(SpaceControl
    src/s_dmalloc.cc
    src/s_permissions.cc
    src/utility.cc
    src/simple_graphs_middleware.cc
)

# Include directories
target_include_directories(SpaceControl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

