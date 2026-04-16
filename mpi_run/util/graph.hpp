#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <unordered_map>
#include <vector>

struct Edge {
    int dst;
    int weight;
};

struct Graph {
    bool directed = true;
    int initial_node_id = 0;
    int node_count = 0;
    std::vector<int> nodes;
    std::unordered_map<int, std::vector<Edge>> adj;
};

#endif