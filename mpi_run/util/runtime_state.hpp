#ifndef RUNTIME_STATE_HPP
#define RUNTIME_STATE_HPP

#include "graph.hpp"
#include "partition.hpp"
#include <unordered_set>
#include <vector>

struct LocalPartitionView {
    int my_rank = 0;
    int world_size = 0;
    std::vector<int> owned_nodes;
    std::unordered_set<int> owned_set;
    std::unordered_set<int> ghost_nodes;
};

#endif