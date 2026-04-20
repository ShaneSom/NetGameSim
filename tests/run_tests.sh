#!/bin/bash

set -u

BIN="/Users/shanesomson/Desktop/NetGameSim/mpi_run/cmake-build-debug/ngs_mpi"

# Base experiment files
GRAPH_BASE="/Users/shanesomson/Desktop/NetGameSim/output/graphs_10_Nodes.json"
PART_BASE="/Users/shanesomson/Desktop/NetGameSim/output/parts_10_Nodes.json"

# Update these if your experiment 2 files use different names
GRAPH_PERTURBED="/Users/shanesomson/Desktop/NetGameSim/output/graphs_10_Nodes_perturbed.json"

# Update these if your experiment 1 alternate partition file uses a different name
PART_ALT="/Users/shanesomson/Desktop/NetGameSim/output/parts_10_Nodes.json"

PASS_COUNT=0
FAIL_COUNT=0

run_pass() {
  echo "PASS: $1"
  PASS_COUNT=$((PASS_COUNT + 1))
}

run_fail() {
  echo "FAIL: $1"
  FAIL_COUNT=$((FAIL_COUNT + 1))
}

require_file() {
  if [ ! -f "$1" ]; then
    echo "Missing required file: $1"
    exit 1
  fi
}

extract_leader() {
  echo "$1" | grep "final leader" | awk '{print $NF}' | tail -n 1
}

extract_dist() {
  local output="$1"
  local node="$2"
  echo "$output" | grep "dist\\[$node\\]" | awk '{print $NF}' | tail -n 1
}

echo "Checking required files..."
require_file "$BIN"
require_file "$GRAPH_BASE"
require_file "$PART_BASE"

echo
echo "=============================="
echo "Test 1: Leader election returns expected leader on base graph"
echo "=============================="

OUT1=$(mpirun -np 2 "$BIN" \
  --graph "$GRAPH_BASE" \
  --part "$PART_BASE" \
  --algo leader 2>&1)

echo "$OUT1"

LEADER1=$(extract_leader "$OUT1")

# Based on your earlier output, expected leader is 10
if [ "$LEADER1" = "10" ]; then
  run_pass "Leader on base graph is 10"
else
  run_fail "Leader on base graph expected 10, got '$LEADER1'"
fi

echo
echo "=============================="
echo "Test 2: Dijkstra on base graph has correct known distances"
echo "=============================="

OUT2=$(mpirun -np 2 "$BIN" \
  --graph "$GRAPH_BASE" \
  --part "$PART_BASE" \
  --algo dijkstra \
  --source 0 2>&1)

echo "$OUT2"

D0=$(extract_dist "$OUT2" 0)
D1=$(extract_dist "$OUT2" 1)
D2=$(extract_dist "$OUT2" 2)
D5=$(extract_dist "$OUT2" 5)

# These values are based on the output you showed earlier.
if [ "$D0" = "0" ] && [ "$D1" = "5" ] && [ "$D2" = "10" ] && [ "$D5" = "14" ]; then
  run_pass "Known Dijkstra distances from source 0 match expected values"
else
  run_fail "Expected dist[0]=0, dist[1]=5, dist[2]=10, dist[5]=14 but got dist[0]=$D0 dist[1]=$D1 dist[2]=$D2 dist[5]=$D5"
fi

echo
echo "=============================="
echo "Test 3: Leader election is consistent across rank counts"
echo "=============================="

OUT3A=$(mpirun -np 2 "$BIN" \
  --graph "$GRAPH_BASE" \
  --part "$PART_BASE" \
  --algo leader 2>&1)

OUT3B=$(mpirun -np 4 "$BIN" \
  --graph "$GRAPH_BASE" \
  --part "$PART_BASE" \
  --algo leader 2>&1)

echo "--- np=2 output ---"
echo "$OUT3A"
echo "--- np=4 output ---"
echo "$OUT3B"

LEADER3A=$(extract_leader "$OUT3A")
LEADER3B=$(extract_leader "$OUT3B")

if [ -n "$LEADER3A" ] && [ "$LEADER3A" = "$LEADER3B" ]; then
  run_pass "Leader is consistent across 2 and 4 ranks ($LEADER3A)"
else
  run_fail "Leader mismatch across rank counts: np=2 -> '$LEADER3A', np=4 -> '$LEADER3B'"
fi

echo
echo "=============================="
echo "Test 4: Same graph, alternate partition gives same leader"
echo "=============================="

if [ -f "$PART_ALT" ]; then
  OUT4A=$(mpirun -np 2 "$BIN" \
    --graph "$GRAPH_BASE" \
    --part "$PART_BASE" \
    --algo leader 2>&1)

  OUT4B=$(mpirun -np 2 "$BIN" \
    --graph "$GRAPH_BASE" \
    --part "$PART_ALT" \
    --algo leader 2>&1)

  echo "--- base partition output ---"
  echo "$OUT4A"
  echo "--- alternate partition output ---"
  echo "$OUT4B"

  LEADER4A=$(extract_leader "$OUT4A")
  LEADER4B=$(extract_leader "$OUT4B")

  if [ -n "$LEADER4A" ] && [ "$LEADER4A" = "$LEADER4B" ]; then
    run_pass "Leader is unchanged across partition strategies ($LEADER4A)"
  else
    run_fail "Leader mismatch across partitions: base -> '$LEADER4A', alt -> '$LEADER4B'"
  fi
else
  echo "SKIP: Alternate partition file not found: $PART_ALT"
fi

echo
echo "=============================="
echo "Test 5: Perturbed graph completes successfully"
echo "=============================="

if [ -f "$GRAPH_PERTURBED" ]; then
  OUT5=$(mpirun -np 2 "$BIN" \
    --graph "$GRAPH_PERTURBED" \
    --part "$PART_BASE" \
    --algo dijkstra \
    --source 0 2>&1)
  STATUS5=$?

  echo "$OUT5"

  if [ $STATUS5 -eq 0 ] && echo "$OUT5" | grep -q "\[dijkstra\] iterations ="; then
    run_pass "Perturbed graph Dijkstra run completed and reported metrics"
  else
    run_fail "Perturbed graph Dijkstra run failed or did not report metrics"
  fi
else
  echo "SKIP: Perturbed graph file not found: $GRAPH_PERTURBED"
fi

echo
echo "=============================="
echo "Test Summary"
echo "=============================="
echo "Passed: $PASS_COUNT"
echo "Failed: $FAIL_COUNT"

if [ "$FAIL_COUNT" -eq 0 ]; then
  echo "ALL EXECUTED TESTS PASSED"
  exit 0
else
  echo "SOME TESTS FAILED"
  exit 1
fi