#ifndef PARTITION_HPP
#define PARTITION_HPP

#include <unordered_map>
#include <vector>

struct BoundaryEdge {
    int src;
    int dst;
    int src_owner;
    int dst_owner;
};

struct Partition {
    int ranks = 0;
    int initial_node_id = 0;
    std::unordered_map<int, int> owner_of;
    std::unordered_map<int, std::vector<int>> owned_nodes_by_rank;
    std::vector<BoundaryEdge> boundary_edges;
};

#endif