#include "feline/index.hpp"

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <queue>
#include <stack>
#include <stdexcept>
#include <vector>

namespace feline {

// Kahn's algorithm: BFS-based topological sort -> X coordinates
static void compute_x_rank(const CSRGraph& dag, FELINEIndex& idx) {
    const uint32_t n = dag.n;
    idx.x_rank.resize(n);

    // Compute in-degrees
    std::vector<uint32_t> in_deg(n, 0);
    for (uint32_t u = 0; u < n; ++u) {
        for (edge_t e = dag.out_begin[u]; e < dag.out_begin[u + 1]; ++e) {
            in_deg[dag.out_adj[e]]++;
        }
    }

    std::queue<vertex_t> q;
    for (uint32_t v = 0; v < n; ++v) {
        if (in_deg[v] == 0) q.push(v);
    }

    uint32_t rank = 0;
    while (!q.empty()) {
        vertex_t u = q.front();
        q.pop();
        idx.x_rank[u] = rank++;
        for (edge_t e = dag.out_begin[u]; e < dag.out_begin[u + 1]; ++e) {
            vertex_t w = dag.out_adj[e];
            if (--in_deg[w] == 0) q.push(w);
        }
    }

    if (rank != n) {
        throw std::runtime_error("Graph contains a cycle (not a DAG)");
    }
}

// Kornaropoulos heuristic: second topological ordering -> Y coordinates
// At each step, pick the root with the highest X rank.
static void compute_y_rank(const CSRGraph& dag, FELINEIndex& idx) {
    const uint32_t n = dag.n;
    idx.y_rank.resize(n);

    // Compute in-degrees
    std::vector<uint32_t> in_deg(n, 0);
    for (uint32_t u = 0; u < n; ++u) {
        for (edge_t e = dag.out_begin[u]; e < dag.out_begin[u + 1]; ++e) {
            in_deg[dag.out_adj[e]]++;
        }
    }

    // Max-heap: (x_rank, vertex)
    using pq_elem = std::pair<uint32_t, vertex_t>;
    std::priority_queue<pq_elem> heap;

    for (uint32_t v = 0; v < n; ++v) {
        if (in_deg[v] == 0) {
            heap.push({idx.x_rank[v], v});
        }
    }

    uint32_t rank = 0;
    while (!heap.empty()) {
        auto [xr, u] = heap.top();
        heap.pop();
        idx.y_rank[u] = rank++;

        for (edge_t e = dag.out_begin[u]; e < dag.out_begin[u + 1]; ++e) {
            vertex_t w = dag.out_adj[e];
            if (--in_deg[w] == 0) {
                heap.push({idx.x_rank[w], w});
            }
        }
    }
}

// Iterative DFS to build spanning tree + min-post intervals (positive-cut filter)
static void compute_spanning_tree_intervals(const CSRGraph& dag, FELINEIndex& idx) {
    const uint32_t n = dag.n;
    idx.interval_start.resize(n);
    idx.interval_end.resize(n);

    std::vector<vertex_t> parent(n, INVALID_VERTEX);
    std::vector<bool> visited(n, false);
    std::vector<std::vector<vertex_t>> tree_children(n);

    // Iterative DFS to find spanning tree and post-order
    struct Frame {
        vertex_t v;
        edge_t child_pos;
    };

    std::stack<Frame> stk;
    std::vector<vertex_t> post_order;
    post_order.reserve(n);

    for (uint32_t root = 0; root < n; ++root) {
        if (visited[root]) continue;
        visited[root] = true;
        stk.push({root, dag.out_begin[root]});

        while (!stk.empty()) {
            auto& [v, pos] = stk.top();
            edge_t end = dag.out_begin[v + 1];

            if (pos < end) {
                vertex_t w = dag.out_adj[pos];
                pos++;
                if (!visited[w]) {
                    visited[w] = true;
                    parent[w] = v;
                    tree_children[v].push_back(w);
                    stk.push({w, dag.out_begin[w]});
                }
            } else {
                // All children processed: assign post-order
                post_order.push_back(v);
                stk.pop();
            }
        }
    }

    // Assign post-order numbers
    for (uint32_t i = 0; i < n; ++i) {
        idx.interval_end[post_order[i]] = i;
    }

    // Compute interval_start bottom-up (in post-order)
    for (uint32_t i = 0; i < n; ++i) {
        vertex_t v = post_order[i];
        if (tree_children[v].empty()) {
            // Leaf: interval is [post, post]
            idx.interval_start[v] = idx.interval_end[v];
        } else {
            // Internal node: min of children's interval_start
            uint32_t min_start = idx.interval_end[v];
            for (vertex_t c : tree_children[v]) {
                min_start = std::min(min_start, idx.interval_start[c]);
            }
            idx.interval_start[v] = min_start;
        }
    }
}

// Level filter: longest path distance from any root
static void compute_levels(const CSRGraph& dag, FELINEIndex& idx) {
    const uint32_t n = dag.n;
    idx.level.resize(n, 0);

    // Build topological order from x_rank (invert: position -> vertex)
    std::vector<vertex_t> topo_order(n);
    for (uint32_t v = 0; v < n; ++v) {
        topo_order[idx.x_rank[v]] = v;
    }

    // Process in topological order
    for (uint32_t i = 0; i < n; ++i) {
        vertex_t u = topo_order[i];
        for (edge_t e = dag.out_begin[u]; e < dag.out_begin[u + 1]; ++e) {
            vertex_t w = dag.out_adj[e];
            idx.level[w] = std::max(idx.level[w], idx.level[u] + 1);
        }
    }
}

FELINEIndex build_index(const CSRGraph& dag) {
    FELINEIndex idx;
    compute_x_rank(dag, idx);
    compute_y_rank(dag, idx);
    compute_spanning_tree_intervals(dag, idx);
    compute_levels(dag, idx);
    return idx;
}

void export_index_csv(const FELINEIndex& idx, const std::string& filename) {
    FILE* f = std::fopen(filename.c_str(), "w");
    if (!f) throw std::runtime_error("Cannot open output file: " + filename);

    std::fprintf(f, "vertex,x,y,level\n");
    uint32_t n = static_cast<uint32_t>(idx.x_rank.size());
    for (uint32_t v = 0; v < n; ++v) {
        std::fprintf(f, "%u,%u,%u,%u\n", v, idx.x_rank[v], idx.y_rank[v], idx.level[v]);
    }
    std::fclose(f);
}

} // namespace feline
