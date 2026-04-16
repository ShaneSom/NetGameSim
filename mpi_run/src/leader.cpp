#include "graph.hpp"
#include "partition.hpp"
#include "runtime_state.hpp"
#include "metrics.hpp"
#include <mpi.h>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <chrono>

struct LeaderResult {
    int leader_id = -1;
    Metrics metrics;
};

LeaderResult run_leader_election(
    const Graph& g,
    const Partition& p,
    const LocalPartitionView& view
) {
    using clock = std::chrono::steady_clock;
    auto start = clock::now();

    std::unordered_map<int, int> candidate;
    for (int node : view.owned_nodes) {
        candidate[node] = node;
    }

    bool global_changed = true;
    int final_leader = -1;
    Metrics metrics;

    while (global_changed) {
        metrics.iterations++;

        std::unordered_map<int, int> next_candidate = candidate;
        std::vector<int> outbound;

        for (int u : view.owned_nodes) {
            auto adj_it = g.adj.find(u);
            if (adj_it == g.adj.end()) continue;

            for (const auto& edge : adj_it->second) {
                int v = edge.dst;
                int cand = candidate[u];
                int owner_v = p.owner_of.at(v);

                if (view.owned_set.count(v)) {
                    if (cand > next_candidate[v]) {
                        next_candidate[v] = cand;
                    }
                } else {
                    outbound.push_back(v);
                    outbound.push_back(cand);
                    metrics.messages_sent++;
                    metrics.bytes_sent += 2 * sizeof(int);
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

        std::vector<int> inbound(total_recv);
        MPI_Allgatherv(
            outbound.data(), out_count, MPI_INT,
            inbound.data(), recv_counts.data(), displs.data(), MPI_INT,
            MPI_COMM_WORLD
        );

        for (int i = 0; i + 1 < total_recv; i += 2) {
            int target = inbound[i];
            int cand = inbound[i + 1];
            if (view.owned_set.count(target) && cand > next_candidate[target]) {
                next_candidate[target] = cand;
            }
        }

        bool local_changed = false;
        for (int node : view.owned_nodes) {
            if (next_candidate[node] != candidate[node]) {
                local_changed = true;
            }
            candidate[node] = next_candidate[node];
            if (candidate[node] > final_leader) final_leader = candidate[node];
        }

        int local_flag = local_changed ? 1 : 0;
        int global_flag = 0;
        MPI_Allreduce(&local_flag, &global_flag, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
        global_changed = (global_flag != 0);
    }

    int agreed_leader = -1;
    MPI_Allreduce(&final_leader, &agreed_leader, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    auto end = clock::now();
    metrics.runtime_seconds =
        std::chrono::duration<double>(end - start).count();

    return {agreed_leader, metrics};
}