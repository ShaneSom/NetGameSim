#!/bin/bash

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXEC="$PROJECT_ROOT/mpi_run/cmake-build-debug/ngs_mpi"
GRAPH="$PROJECT_ROOT/output/graphs_10_nodes.json"
PART="$PROJECT_ROOT/output/parts_10_Nodes.json"
RESULTS_DIR="$PROJECT_ROOT/experiment/results"

mkdir -p "$RESULTS_DIR"

echo "Running Leader on 10-node graph..."
mpirun -np 2 "$EXEC" --graph "$GRAPH" --part "$PART" --algo leader \
  | tee "$RESULTS_DIR/leader_10_nodes.txt"

echo ""
echo "Running Dijkstra on 10-node graph..."
mpirun -np 2 "$EXEC" --graph "$GRAPH" --part "$PART" --algo dijkstra \
  | tee "$RESULTS_DIR/dijkstra_10_nodes.txt"