// Observation tool: incrementally builds a Feline-PK dynamic index by inserting
// all vertices and edges of a real (static-test) graph one at a time, timing each
// edge insertion and tracking SCC folds (drops in the number of DAG representatives).
// Finishes with a correctness check against a BFS oracle over the final digraph.
//
// Usage: dyn_incremental <graph_file> [num_sample_queries=2000] [seed=42]
#include "feline/graph.hpp"
#include "feline_dyn/feline_pk.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <random>
#include <unordered_set>
#include <vector>

using clock_t_ = std::chrono::steady_clock;

// Self-contained BFS oracle directly over the CSR adjacency (avoids building a
// second DynamicGraph copy of a potentially large graph).
static bool bfs_reachable_csr(const feline::CSRGraph& g, feline::vertex_t u,
                               feline::vertex_t v) {
    if (u == v) return true;
    std::vector<bool> visited(g.n, false);
    std::queue<feline::vertex_t> q;
    visited[u] = true;
    q.push(u);
    while (!q.empty()) {
        feline::vertex_t cur = q.front();
        q.pop();
        for (feline::edge_t e = g.out_begin[cur]; e < g.out_begin[cur + 1]; ++e) {
            feline::vertex_t w = g.out_adj[e];
            if (w == v) return true;
            if (!visited[w]) {
                visited[w] = true;
                q.push(w);
            }
        }
    }
    return false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <graph_file> [num_sample_queries=2000] [seed=42]\n",
                     argv[0]);
        return 2;
    }
    const std::string path = argv[1];
    const uint64_t num_sample_queries = (argc >= 3) ? std::strtoull(argv[2], nullptr, 10) : 2000;
    const uint32_t seed = (argc >= 4) ? static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10)) : 42;

    feline::CSRGraph g = feline::load_graph(path);
    std::printf("Loaded graph: %s (n=%u, m=%llu)\n", path.c_str(), g.n,
                static_cast<unsigned long long>(g.m));

    feline_dyn::FelinePK pk;
    for (feline::vertex_t v = 0; v < g.n; ++v) {
        pk.insert_vertex(v);
    }

    // Progress cadence: report about 10 times over the course of insertion.
    const uint64_t progress_every = std::max<uint64_t>(1, g.m / 10);

    uint64_t folds = 0;
    uint64_t reps_absorbed = 0;
    uint64_t edges_done = 0;
    double total_time_us = 0.0;
    double max_time_us = 0.0;
    // Number of representatives = number of vertices currently in the DAG.
    // (index().size() counts every vertex ever inserted: folded members keep coordinates.)
    uint32_t prev_reps = static_cast<uint32_t>(pk.graph().dag_out_all().size());

    for (feline::vertex_t u = 0; u < g.n; ++u) {
        for (feline::edge_t e = g.out_begin[u]; e < g.out_begin[u + 1]; ++e) {
            feline::vertex_t v = g.out_adj[e];

            auto t0 = clock_t_::now();
            pk.insert_edge(u, v);
            auto t1 = clock_t_::now();
            double dt_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

            total_time_us += dt_us;
            if (dt_us > max_time_us) max_time_us = dt_us;

            const size_t reps = pk.graph().dag_out_all().size();
            uint32_t cur_reps = static_cast<uint32_t>(reps);
            if (cur_reps < prev_reps) {
                folds++;
                reps_absorbed += (prev_reps - cur_reps);
            }
            prev_reps = cur_reps;

            edges_done++;
            if (edges_done % progress_every == 0 || edges_done == g.m) {
                double avg_us = total_time_us / static_cast<double>(edges_done);
                std::printf(
                    "edges inserted %llu/%llu | reps=%u | folds so far=%llu | avg update=%.3f us\n",
                    static_cast<unsigned long long>(edges_done),
                    static_cast<unsigned long long>(g.m), cur_reps,
                    static_cast<unsigned long long>(folds), avg_us);
            }
        }
    }

    double avg_time_us = (g.m > 0) ? total_time_us / static_cast<double>(g.m) : 0.0;

    std::printf("\n=== SUMMARY ===\n");
    std::printf("n = %u\n", g.n);
    std::printf("m = %llu\n", static_cast<unsigned long long>(g.m));
    std::printf("final reps = %u\n", static_cast<uint32_t>(pk.graph().dag_out_all().size()));
    std::printf("total folds = %llu (reps absorbed = %llu)\n",
                static_cast<unsigned long long>(folds),
                static_cast<unsigned long long>(reps_absorbed));
    std::printf("total insert_edge time = %.3f ms\n", total_time_us / 1000.0);
    std::printf("avg update = %.3f us | max update = %.3f us\n", avg_time_us, max_time_us);

    // Correctness observation: BFS oracle over the final digraph.
    std::printf("\n=== CORRECTNESS ===\n");
    uint64_t total_checked = 0;
    uint64_t correct = 0;
    struct Mismatch { feline::vertex_t a, b; bool expected, got; };
    std::vector<Mismatch> mismatches;

    auto check_pair = [&](feline::vertex_t a, feline::vertex_t b) {
        bool expected = bfs_reachable_csr(g, a, b);
        bool got = pk.reachable(a, b);
        total_checked++;
        if (expected == got) {
            correct++;
        } else if (mismatches.size() < 5) {
            mismatches.push_back({a, b, expected, got});
        }
    };

    if (g.n <= 50) {
        for (feline::vertex_t a = 0; a < g.n; ++a) {
            for (feline::vertex_t b = 0; b < g.n; ++b) {
                check_pair(a, b);
            }
        }
    } else {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<feline::vertex_t> dist(0, g.n - 1);
        for (uint64_t i = 0; i < num_sample_queries; ++i) {
            feline::vertex_t a = dist(rng);
            feline::vertex_t b = dist(rng);
            check_pair(a, b);
        }
    }

    double pct = (total_checked > 0)
                     ? (100.0 * static_cast<double>(correct) / static_cast<double>(total_checked))
                     : 100.0;
    std::printf("correctness: %llu/%llu (%.2f%%)\n",
                static_cast<unsigned long long>(correct),
                static_cast<unsigned long long>(total_checked), pct);
    if (!mismatches.empty()) {
        std::printf("mismatch examples (up to 5):\n");
        for (const auto& mm : mismatches) {
            std::printf("  (%u,%u): expected=%d got=%d\n", mm.a, mm.b, (int)mm.expected,
                        (int)mm.got);
        }
    }

    return 0;
}
