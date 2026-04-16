#include "mpi_run/util/graph.hpp"
#include "partition.hpp"
#include "runtime_state.hpp"
#include "metrics.hpp"
#include <mpi.h>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

Graph load_graph(const std::string& path);
Partition load_partition(const std::string& path);
LocalPartitionView build_local_view(const Graph& g, const Partition& p, int my_rank, int world_size);

struct LeaderResult {
    int leader_id;
    Metrics metrics;
};
LeaderResult run_leader_election(const Graph& g, const Partition& p, const LocalPartitionView& view);

struct DijkstraResult {
    std::unordered_map<int, long long> distances;
    Metrics metrics;
};
DijkstraResult run_distributed_dijkstra(const Graph& g, const Partition& p, const LocalPartitionView& view, int source);

static std::string get_arg(int argc, char** argv, const std::string& flag, const std::string& def = "") {
    for (int i = 1; i + 1 < argc; i++) {
        if (argv[i] == flag) return argv[i + 1];
    }
    return def;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0, size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    try {
        std::string graph_path = get_arg(argc, argv, "--graph");
        std::string part_path = get_arg(argc, argv, "--part");
        std::string algo = get_arg(argc, argv, "--algo");
        std::string source_str = get_arg(argc, argv, "--source", "0");

        if (graph_path.empty() || part_path.empty() || algo.empty()) {
            if (rank == 0) {
                std::cerr << "Usage: --graph graph.json --part part.json --algo leader|dijkstra [--source 0]\n";
            }
            MPI_Finalize();
            return 1;
        }

        Graph g = load_graph(graph_path);
        Partition p = load_partition(part_path);
        LocalPartitionView view = build_local_view(g, p, rank, size);

        std::cout << "Rank " << rank << " owns:";
        for (int n : view.owned_nodes) std::cout << " " << n;
        std::cout << "\n";

        std::cout << "Rank " << rank << " ghost nodes:";
        for (int n : view.ghost_nodes) std::cout << " " << n;
        std::cout << "\n";

        if (algo == "leader") {
            auto result = run_leader_election(g, p, view);
            if (rank == 0) {
                std::cout << "[leader] final leader = " << result.leader_id << "\n";
                std::cout << "[leader] iterations = " << result.metrics.iterations << "\n";
                std::cout << "[leader] messages = " << result.metrics.messages_sent << "\n";
                std::cout << "[leader] bytes = " << result.metrics.bytes_sent << "\n";
                std::cout << "[leader] runtime = " << result.metrics.runtime_seconds << " s\n";
            }
        } else if (algo == "dijkstra") {
            int source = std::stoi(source_str);
            auto result = run_distributed_dijkstra(g, p, view, source);

            for (const auto& [node, d] : result.distances) {
                std::cout << "Rank " << rank << " dist[" << node << "] = " << d << "\n";
            }

            if (rank == 0) {
                std::cout << "[dijkstra] iterations = " << result.metrics.iterations << "\n";
                std::cout << "[dijkstra] messages = " << result.metrics.messages_sent << "\n";
                std::cout << "[dijkstra] bytes = " << result.metrics.bytes_sent << "\n";
                std::cout << "[dijkstra] runtime = " << result.metrics.runtime_seconds << " s\n";
            }
        } else {
            if (rank == 0) {
                std::cerr << "Unknown algo: " << algo << "\n";
            }
            MPI_Finalize();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Rank " << rank << " error: " << e.what() << "\n";
        MPI_Finalize();
        return 1;
    }

    MPI_Finalize();
    return 0;
}