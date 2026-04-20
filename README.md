# NetGameSim Distributed Graph Algorithms

## Overview

This project implements distributed graph algorithms using MPI. The system operates on graphs generated or imported through NetGameSim, partitions the graph across multiple ranks, and performs computation using message passing.

Implemented algorithms:
- Leader Election (FloodMax)
- Distributed Dijkstra Shortest Path

---

## Dependencies

Make sure the following are installed:

- C++ compiler (clang++ or g++)
- CMake (version 3.10+)
- MPI implementation (OpenMPI or MPICH)
- macOS or Linux environment

Verify MPI installation:

```bash
mpirun --version
```

## Build Instructions
```bash
cd <Path-to-mpi_run>
mkdir -p build
cd build
cmake ..
make
```

## End to End
```bash
mpirun -np 2 <path-to-ngs_mpi> \
--graph <path-to-graphs.json> \
--part <path-to-parts.json> \
--algo <algorithm leader/dijkstra>
```
