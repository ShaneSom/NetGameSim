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

struct DijkstraResult {
    std::unordered_map<int, long long> distances;
    Metrics metrics;
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

    if (view.owned_set.count(source)) {
        dist[source] = 0;
    }

    while (true) {
        metrics.iterations++;

        long long local_best_dist = INF;
        int local_best_node = -1;

        for (int node : view.owned_nodes) {
            if (!settled[node] && dist[node] < local_best_dist) {
                local_best_dist = dist[node];
                local_best_node = node;
            }
        }

        struct {
            long long dist;
            int node;
        } local_pair{local_best_dist, local_best_node}, global_pair{INF, -1};

        MPI_Allreduce(&local_pair, &global_pair, 1, MPI_LONG_LONG_INT, MPI_MIN, MPI_COMM_WORLD);

        // Fallback termination
        long long min_dist = local_best_dist;
        long long global_min = INF;
        MPI_Allreduce(&min_dist, &global_min, 1, MPI_LONG_LONG, MPI_MIN, MPI_COMM_WORLD);
        if (global_min >= INF) break;

        int chosen_node = -1;
        long long chosen_dist = INF;

        for (int node : view.owned_nodes) {
            if (!settled[node] && dist[node] == global_min) {
                chosen_node = node;
                chosen_dist = dist[node];
                break;
            }
        }

        int global_node = -1;
        MPI_Allreduce(&chosen_node, &global_node, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
        chosen_node = global_node;
        chosen_dist = global_min;

        if (view.owned_set.count(chosen_node)) {
            settled[chosen_node] = true;
        }

        std::vector<long long> outbound;

        auto adj_it = g.adj.find(chosen_node);
        if (adj_it != g.adj.end()) {
            for (const auto& edge : adj_it->second) {
                long long proposal = chosen_dist + edge.weight;
                int owner_v = p.owner_of.at(edge.dst);

                if (view.owned_set.count(edge.dst)) {
                    if (!settled[edge.dst] && proposal < dist[edge.dst]) {
                        dist[edge.dst] = proposal;
                    }
                } else {
                    outbound.push_back(edge.dst);
                    outbound.push_back(proposal);
                    metrics.messages_sent++;
                    metrics.bytes_sent += sizeof(int) + sizeof(long long);
                }
            }
        }

        int out_count = static_cast<int>(outbound.size());
        std::vector<int> recv_counts(view.world_size, 0);
        MPI_Allgather(&out_count, 1, MPI_INT, recv_counts.data(), 1, MPI_INT, MPI_COMM_WORLD);

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

        for (int i = 0; i + 1 < total_recv; i += 2) {
            int target = static_cast<int>(inbound[i]);
            long long proposal = inbound[i + 1];
            if (view.owned_set.count(target) && !settled[target] && proposal < dist[target]) {
                dist[target] = proposal;
            }
        }
    }

    auto end = clock::now();
    metrics.runtime_seconds =
        std::chrono::duration<double>(end - start).count();

    return {dist, metrics};
}