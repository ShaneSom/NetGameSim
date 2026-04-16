#include "graph.hpp"
#include "partition.hpp"
#include "runtime_state.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Graph load_graph(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Failed to open graph file: " + path);

    json j;
    in >> j;

    Graph g;
    g.directed = j.at("directed").get<bool>();
    g.initial_node_id = j.at("initialNodeId").get<int>();
    g.node_count = j.at("nodeCount").get<int>();
    g.nodes = j.at("nodes").get<std::vector<int>>();

    for (const auto& e : j.at("edges")) {
        int src = e.at("src").get<int>();
        int dst = e.at("dst").get<int>();
        int weight = e.at("weight").get<int>();
        g.adj[src].push_back({dst, weight});
        if (!g.directed) {
            g.adj[dst].push_back({src, weight});
        }
    }

    return g;
}

Partition load_partition(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Failed to open partition file: " + path);

    json j;
    in >> j;

    Partition p;
    p.ranks = j.at("ranks").get<int>();
    p.initial_node_id = j.at("initialNodeId").get<int>();

    for (auto it = j.at("ownerOf").begin(); it != j.at("ownerOf").end(); ++it) {
        int node = std::stoi(it.key());
        int rank = it.value().get<int>();
        p.owner_of[node] = rank;
    }

    for (auto it = j.at("ownedNodesByRank").begin(); it != j.at("ownedNodesByRank").end(); ++it) {
        int rank = std::stoi(it.key());
        p.owned_nodes_by_rank[rank] = it.value().get<std::vector<int>>();
    }

    if (j.contains("boundaryEdges")) {
        for (const auto& e : j.at("boundaryEdges")) {
            p.boundary_edges.push_back({
                e.at("src").get<int>(),
                e.at("dst").get<int>(),
                e.at("srcOwner").get<int>(),
                e.at("dstOwner").get<int>()
            });
        }
    }

    return p;
}

LocalPartitionView build_local_view(const Graph& g, const Partition& p, int my_rank, int world_size) {
    LocalPartitionView view;
    view.my_rank = my_rank;
    view.world_size = world_size;

    auto it = p.owned_nodes_by_rank.find(my_rank);
    if (it != p.owned_nodes_by_rank.end()) {
        view.owned_nodes = it->second;
        for (int n : view.owned_nodes) view.owned_set.insert(n);
    }

    for (int u : view.owned_nodes) {
        auto adj_it = g.adj.find(u);
        if (adj_it == g.adj.end()) continue;
        for (const auto& edge : adj_it->second) {
            if (!view.owned_set.count(edge.dst)) {
                view.ghost_nodes.insert(edge.dst);
            }
        }
    }

    return view;
}