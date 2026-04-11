#include "feline/query.hpp"

#include <cstdio>
#include <cstdlib>
#include <queue>
#include <random>
#include <stdexcept>
#include <vector>

namespace feline {

bool reachable(vertex_t u, vertex_t v, const CSRGraph& dag, const FELINEIndex& idx) {
    // Iterative DFS implementing Algorithm 3 from the paper
    struct Frame {
        vertex_t node;
        edge_t child_pos;
    };
    thread_local std::vector<Frame> stack;
    stack.clear();
    stack.push_back({u, dag.out_begin[u]});

    while (!stack.empty()) {
        auto& [node, pos] = stack.back();

        // First visit to this node in the current DFS path
        if (pos == dag.out_begin[node]) {
            // Positive-cut filter: if I_v ⊆ I_node, then node reaches v
            if (idx.interval_start[node] <= idx.interval_start[v] &&
                idx.interval_end[v] <= idx.interval_end[node]) {
                return true;
            }

            // Reflexive check
            if (node == v) return true;

            // Negative-cut (dominance) + level filter
            // Prune if node does not dominate v or same/higher level
            if (!(idx.x_rank[node] <= idx.x_rank[v] &&
                  idx.y_rank[node] <= idx.y_rank[v] &&
                  idx.level[node] < idx.level[v])) {
                stack.pop_back();
                continue;
            }
        }

        // Try next child
        if (pos < dag.out_begin[node + 1]) {
            vertex_t w = dag.out_adj[pos];
            pos++;
            stack.push_back({w, dag.out_begin[w]});
        } else {
            // All children exhausted, backtrack
            stack.pop_back();
        }
    }

    return false;
}

uint64_t process_queries(const std::string& query_file,
                         const std::string& output_file,
                         const CSRGraph& dag,
                         const FELINEIndex& idx,
                         const std::vector<vertex_t>& comp_id) {
    FILE* fin = std::fopen(query_file.c_str(), "r");
    if (!fin) throw std::runtime_error("Cannot open query file: " + query_file);

    FILE* fout = nullptr;
    if (!output_file.empty()) {
        fout = std::fopen(output_file.c_str(), "w");
        if (!fout) {
            std::fclose(fin);
            throw std::runtime_error("Cannot open output file: " + output_file);
        }
    }

    uint64_t count = 0;
    uint32_t u, v;
    while (std::fscanf(fin, "%u %u", &u, &v) == 2) {
        vertex_t cu = comp_id[u];
        vertex_t cv = comp_id[v];

        bool result;
        if (cu == cv) {
            // Same SCC: trivially reachable
            result = true;
        } else {
            result = reachable(cu, cv, dag, idx);
        }

        if (fout) {
            std::fprintf(fout, "%d\n", result ? 1 : 0);
        }
        count++;
    }

    std::fclose(fin);
    if (fout) std::fclose(fout);
    return count;
}

bool naive_reachable(vertex_t u, vertex_t v, const CSRGraph& g) {
    if (u == v) return true;
    std::vector<bool> visited(g.n, false);
    std::queue<vertex_t> q;
    visited[u] = true;
    q.push(u);
    while (!q.empty()) {
        vertex_t cur = q.front(); q.pop();
        for (edge_t e = g.out_begin[cur]; e < g.out_begin[cur + 1]; ++e) {
            vertex_t w = g.out_adj[e];
            if (w == v) return true;
            if (!visited[w]) {
                visited[w] = true;
                q.push(w);
            }
        }
    }
    return false;
}

uint64_t gen_queries(const std::string& output_file,
                     const CSRGraph& g,
                     uint64_t num_queries,
                     uint32_t seed) {
    FILE* fout = std::fopen(output_file.c_str(), "w");
    if (!fout) throw std::runtime_error("Cannot open output file: " + output_file);

    std::mt19937 rng(seed);
    std::uniform_int_distribution<uint32_t> dist(0, g.n - 1);

    for (uint64_t i = 0; i < num_queries; ++i) {
        vertex_t u = dist(rng);
        vertex_t v = dist(rng);
        bool result = naive_reachable(u, v, g);
        std::fprintf(fout, "%u %u %d\n", u, v, result ? 1 : 0);
    }

    std::fclose(fout);
    return num_queries;
}

VerifyResult verify_queries(const std::string& query_file,
                            const CSRGraph& dag,
                            const FELINEIndex& idx,
                            const std::vector<vertex_t>& comp_id) {
    FILE* fin = std::fopen(query_file.c_str(), "r");
    if (!fin) throw std::runtime_error("Cannot open query file: " + query_file);

    VerifyResult res;
    uint32_t u, v;
    int expected;

    while (std::fscanf(fin, "%u %u %d", &u, &v, &expected) == 3) {
        vertex_t cu = comp_id[u];
        vertex_t cv = comp_id[v];

        bool feline_result;
        if (cu == cv) {
            feline_result = true;
        } else {
            feline_result = reachable(cu, cv, dag, idx);
        }

        res.total++;
        if (expected == 1) res.total_positive++;
        else res.total_negative++;

        if (feline_result == (expected == 1)) {
            res.correct++;
        } else {
            res.wrong++;
            if (feline_result && expected == 0) {
                res.false_positives++;
            } else {
                res.false_negatives++;
            }
        }
    }

    std::fclose(fin);
    return res;
}

} // namespace feline
