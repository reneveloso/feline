#include "feline/graph.hpp"

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <stdexcept>
#include <stack>
#include <vector>

namespace feline {

CSRGraph load_graph(const std::string& filename) {
    FILE* f = std::fopen(filename.c_str(), "r");
    if (!f) throw std::runtime_error("Cannot open file: " + filename);

    uint32_t n;
    uint64_t m;
    if (std::fscanf(f, "%u %lu", &n, &m) != 2) {
        std::fclose(f);
        throw std::runtime_error("Invalid header in: " + filename);
    }

    // Two-pass CSR construction
    // Pass 1: count out-degrees
    std::vector<edge_t> degree(n, 0);
    std::vector<std::pair<vertex_t, vertex_t>> edges(m);

    for (uint64_t i = 0; i < m; ++i) {
        uint32_t u, v;
        if (std::fscanf(f, "%u %u", &u, &v) != 2) {
            std::fclose(f);
            throw std::runtime_error("Invalid edge at line " + std::to_string(i + 2));
        }
        edges[i] = {u, v};
        degree[u]++;
    }
    std::fclose(f);

    // Prefix sum to build out_begin
    CSRGraph g;
    g.n = n;
    g.m = m;
    g.out_begin.resize(n + 1, 0);
    for (uint32_t i = 0; i < n; ++i) {
        g.out_begin[i + 1] = g.out_begin[i] + degree[i];
    }

    // Pass 2: fill out_adj
    g.out_adj.resize(m);
    std::vector<edge_t> cursor(g.out_begin.begin(), g.out_begin.begin() + n);
    for (uint64_t i = 0; i < m; ++i) {
        auto [u, v] = edges[i];
        g.out_adj[cursor[u]++] = v;
    }

    return g;
}

CondensationResult condense(const CSRGraph& g) {
    const uint32_t n = g.n;
    constexpr uint32_t UNVISITED = UINT32_MAX;

    std::vector<uint32_t> index(n, UNVISITED);
    std::vector<uint32_t> lowlink(n, 0);
    std::vector<bool> on_stack(n, false);
    std::vector<vertex_t> comp_id(n, UNVISITED);

    std::stack<vertex_t> S; // Tarjan's SCC stack
    uint32_t idx_counter = 0;
    uint32_t comp_counter = 0;

    // Iterative Tarjan
    // Each frame: (vertex, next_child_position)
    struct Frame {
        vertex_t v;
        edge_t child_pos;
    };
    std::stack<Frame> call_stack;

    for (uint32_t root = 0; root < n; ++root) {
        if (index[root] != UNVISITED) continue;

        call_stack.push({root, g.out_begin[root]});
        index[root] = lowlink[root] = idx_counter++;
        S.push(root);
        on_stack[root] = true;

        while (!call_stack.empty()) {
            auto& [u, pos] = call_stack.top();
            edge_t end = g.out_begin[u + 1];

            if (pos < end) {
                vertex_t w = g.out_adj[pos];
                pos++;
                if (index[w] == UNVISITED) {
                    index[w] = lowlink[w] = idx_counter++;
                    S.push(w);
                    on_stack[w] = true;
                    call_stack.push({w, g.out_begin[w]});
                } else if (on_stack[w]) {
                    lowlink[u] = std::min(lowlink[u], index[w]);
                }
            } else {
                // All children processed
                if (lowlink[u] == index[u]) {
                    // Found an SCC root: pop the component
                    vertex_t w;
                    do {
                        w = S.top();
                        S.pop();
                        on_stack[w] = false;
                        comp_id[w] = comp_counter;
                    } while (w != u);
                    comp_counter++;
                }
                call_stack.pop();
                if (!call_stack.empty()) {
                    lowlink[call_stack.top().v] =
                        std::min(lowlink[call_stack.top().v], lowlink[u]);
                }
            }
        }
    }

    // If already a DAG (each SCC is a single vertex), return with identity-like mapping
    // Note: comp_id from Tarjan is in reverse topological order, remap to [0..n)
    // Remap comp_ids so that they form a topological order of the DAG
    // Tarjan gives SCCs in reverse finishing order, so reverse the mapping
    uint32_t num_comp = comp_counter;
    for (uint32_t i = 0; i < n; ++i) {
        comp_id[i] = num_comp - 1 - comp_id[i];
    }

    if (num_comp == n) {
        // Already a DAG, reuse original graph with remapped vertex ids
        // Build a permutation-based CSR
        // Actually, since comp_id is now a permutation, we need to rebuild
        // For simplicity, rebuild the DAG with the new vertex ids
    }

    // Build condensed DAG
    // Collect edges between different components, removing duplicates
    std::vector<std::pair<vertex_t, vertex_t>> dag_edges;
    for (uint32_t u = 0; u < n; ++u) {
        for (edge_t e = g.out_begin[u]; e < g.out_begin[u + 1]; ++e) {
            vertex_t v = g.out_adj[e];
            vertex_t cu = comp_id[u];
            vertex_t cv = comp_id[v];
            if (cu != cv) {
                dag_edges.push_back({cu, cv});
            }
        }
    }

    // Sort and deduplicate
    std::sort(dag_edges.begin(), dag_edges.end());
    dag_edges.erase(std::unique(dag_edges.begin(), dag_edges.end()), dag_edges.end());

    // Build CSR for the DAG
    CSRGraph dag;
    dag.n = num_comp;
    dag.m = dag_edges.size();
    dag.out_begin.resize(num_comp + 1, 0);

    for (auto& [u, v] : dag_edges) {
        dag.out_begin[u + 1]++;
    }
    for (uint32_t i = 0; i < num_comp; ++i) {
        dag.out_begin[i + 1] += dag.out_begin[i];
    }

    dag.out_adj.resize(dag.m);
    std::vector<edge_t> cursor(dag.out_begin.begin(), dag.out_begin.begin() + num_comp);
    for (auto& [u, v] : dag_edges) {
        dag.out_adj[cursor[u]++] = v;
    }

    CondensationResult result;
    result.dag = std::move(dag);
    result.comp_id = std::move(comp_id);
    result.num_components = num_comp;
    return result;
}

} // namespace feline
