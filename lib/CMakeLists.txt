cmake_minimum_required(VERSION 3.9.1)
project(SpaceControl)


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pthread -fopenmp -g")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/)

# Create the library target for projects to include
add_library(SpaceControl
    src/s_dmalloc.cc
    src/s_permissions.cc
    src/utility.cc
)

# Include directories
target_include_directories(SpaceControl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(main
    src/main.cc
    src/s_dmalloc.cc
    src/s_dmalloc.hh
    src/s_permissions.hh
    src/s_permissions.cc
    src/utility.hh
    src/utility.cc)

add_executable(fam_main
    src/fam_main.cc
    src/s_dmalloc.cc
    src/s_dmalloc.hh
    src/s_permissions.hh
    src/s_permissions.cc
    src/utility.hh
    src/utility.cc)