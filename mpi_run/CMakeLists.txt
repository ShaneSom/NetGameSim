cmake_minimum_required(VERSION 3.10)
project(ngs_mpi)

set(CMAKE_CXX_STANDARD 17)

find_package(MPI REQUIRED)

add_executable(ngs_mpi
        src/main.cpp
        src/io.cpp
        src/leader.cpp
        src/dijkstra.cpp
)

target_include_directories(ngs_mpi PRIVATE util)

target_link_libraries(ngs_mpi MPI::MPI_CXX)