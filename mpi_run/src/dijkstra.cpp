#include "graph.hpp"
#include "partition.hpp"
#include "runtime_state.hpp"
#include "metrics.hpp"
#include <mpi.h>
#include <unordered_map>
#include <queue>
#include <vector>
#include <limits>
#include <chrono>
#include <tuple>

struct DijkstraResult {
    std::unordered_map<int, long long> distances;
    Metrics metrics;
};

struct PQEntry {
    long long dist;
    int node;

    bool operator>(const PQEntry& other) const {
        if (dist != other.dist) return dist > other.dist;
        return node > other.node;
    }
};

DijkstraResult run_distributed_dijkstra(
        const Graph& g,
        const Partition& p,
        const LocalPartitionView& view,
        int source
) {
    using clock = std::chrono::steady_clock;
    auto start = clock::now();

    const long long INF = std::numeric_limits<long long>::max() / 4;
    Metrics metrics;

    std::unordered_map<int, long long> dist;
    std::unordered_map<int, bool> settled;

    for (int node : view.owned_nodes) {
        dist[node] = INF;
        settled[node] = false;
    }

    using MinHeap = std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>>;
    MinHeap pq;

    if (view.owned_set.count(source)) {
        dist[source] = 0;
        pq.push({0, source});
    }

    while (true) {
        // Remove stale / already-settled entries from local frontier
        while (!pq.empty()) {
            const auto top = pq.top();
            if (settled[top.node] || top.dist != dist[top.node]) {
                pq.pop();
            } else {
                break;
            }
        }

        long long local_best_dist = INF;
        int local_best_node = -1;

        if (!pq.empty()) {
            local_best_dist = pq.top().dist;
            local_best_node = pq.top().node;
        }

        // Gather each rank's best candidate: [dist, node]
        long long local_candidate[2] = {local_best_dist, static_cast<long long>(local_best_node)};
        std::vector<long long> gathered(2 * view.world_size, -1);

        MPI_Allgather(
                local_candidate, 2, MPI_LONG_LONG,
                gathered.data(), 2, MPI_LONG_LONG,
                MPI_COMM_WORLD
        );

        // Choose the globally best unsettled node.
        // Tie-break by smaller node id for deterministic behavior.
        long long chosen_dist = INF;
        int chosen_node = -1;

        for (int r = 0; r < view.world_size; r++) {
            long long cand_dist = gathered[2 * r];
            int cand_node = static_cast<int>(gathered[2 * r + 1]);

            if (cand_node == -1) continue;

            if (cand_dist < chosen_dist ||
                (cand_dist == chosen_dist && cand_node < chosen_node)) {
                chosen_dist = cand_dist;
                chosen_node = cand_node;
            }
        }

        // No rank has any unsettled reachable node left
        if (chosen_node == -1 || chosen_dist >= INF) {
            break;
        }

        metrics.iterations++;

        // Only the owner of the chosen node settles it and relaxes outgoing edges
        if (view.owned_set.count(chosen_node)) {
            settled[chosen_node] = true;
            pq.pop();
        }

        // Outbound messages encoded as triples:
        // [target_node, proposed_distance, unused_padding]
        // We keep a fixed-width long long format for easy MPI_Allgatherv.
        std::vector<long long> outbound;

        auto adj_it = g.adj.find(chosen_node);
        if (adj_it != g.adj.end()) {
            for (const auto& edge : adj_it->second) {
                long long proposal = chosen_dist + edge.weight;
                int v = edge.dst;

                if (view.owned_set.count(v)) {
                    if (!settled[v] && proposal < dist[v]) {
                        dist[v] = proposal;
                        pq.push({proposal, v});
                    }
                } else {
                    outbound.push_back(static_cast<long long>(v));
                    outbound.push_back(proposal);
                    outbound.push_back(0); // padding for fixed record size

                    metrics.messages_sent++;
                    metrics.bytes_sent += sizeof(int) + sizeof(long long);
                }
            }
        }

        int out_count = static_cast<int>(outbound.size());
        std::vector<int> recv_counts(view.world_size, 0);

        MPI_Allgather(
                &out_count, 1, MPI_INT,
                recv_counts.data(), 1, MPI_INT,
                MPI_COMM_WORLD
        );

        int total_recv = 0;
        std::vector<int> displs(view.world_size, 0);
        for (int i = 0; i < view.world_size; i++) {
            displs[i] = total_recv;
            total_recv += recv_counts[i];
        }

        std::vector<long long> inbound(total_recv);

        MPI_Allgatherv(
                outbound.data(), out_count, MPI_LONG_LONG,
                inbound.data(), recv_counts.data(), displs.data(), MPI_LONG_LONG,
                MPI_COMM_WORLD
        );

        // Apply remote relaxations
        for (int i = 0; i + 2 < total_recv; i += 3) {
            int target = static_cast<int>(inbound[i]);
            long long proposal = inbound[i + 1];

            if (view.owned_set.count(target) &&
                !settled[target] &&
                proposal < dist[target]) {
                dist[target] = proposal;
                pq.push({proposal, target});
            }
        }
    }

    auto end = clock::now();
    metrics.runtime_seconds =
            std::chrono::duration<double>(end - start).count();

    return {dist, metrics};
}