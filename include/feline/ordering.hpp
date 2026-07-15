#pragma once

#include <cstdint>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace feline {

struct XYOrdering {
    std::vector<uint32_t> x_rank; // DFS topological order (reverse postorder)
    std::vector<uint32_t> y_rank; // Kornaropoulos heuristic
};

// Computes X and Y ranks for a DAG over vertices 0..n-1.
// for_each_succ(u, emit) must call emit(w) for every successor w of u.
// Throws std::runtime_error if the successor relation contains a cycle.
template <typename ForEachSucc>
XYOrdering compute_xy_ordering(uint32_t n, ForEachSucc&& for_each_succ) {
    // 1) Materialize a local CSR from the callback (callback used exactly twice).
    // Offsets are size_t (edge count can exceed uint32_t range); adj VALUES are
    // vertex ids and stay uint32_t.
    std::vector<size_t> begin(static_cast<size_t>(n) + 1, 0);
    for (uint32_t u = 0; u < n; ++u) {
        uint32_t deg = 0;
        for_each_succ(u, [&](uint32_t) { ++deg; });
        begin[u + 1] = deg;
    }
    for (uint32_t u = 0; u < n; ++u) begin[u + 1] += begin[u];

    std::vector<uint32_t> adj(begin[n]);
    {
        std::vector<size_t> cursor(begin.begin(), begin.end());
        for (uint32_t u = 0; u < n; ++u) {
            for_each_succ(u, [&](uint32_t w) { adj[cursor[u]++] = w; });
        }
    }

    XYOrdering out;
    out.x_rank.assign(n, 0);

    // 2) X = iterative DFS reverse-postorder, with 3-color cycle detection.
    std::vector<uint8_t> color(n, 0); // 0=white, 1=gray, 2=black
    std::vector<uint32_t> post;
    post.reserve(n);
    std::vector<size_t> cur(begin.begin(), begin.begin() + n); // per-node cursor
    std::vector<uint32_t> stk;
    for (uint32_t s = 0; s < n; ++s) {
        if (color[s] != 0) continue;
        color[s] = 1;
        stk.push_back(s);
        while (!stk.empty()) {
            uint32_t v = stk.back();
            if (cur[v] < begin[v + 1]) {
                uint32_t w = adj[cur[v]++];
                if (color[w] == 0) {
                    color[w] = 1;
                    stk.push_back(w);
                } else if (color[w] == 1) {
                    throw std::runtime_error("Graph contains a cycle (not a DAG)");
                }
                // color[w] == 2 (black): already finished, skip
            } else {
                color[v] = 2;
                post.push_back(v);
                stk.pop_back();
            }
        }
    }
    for (uint32_t i = 0; i < n; ++i) {
        out.x_rank[post[n - 1 - i]] = i; // reverse postorder
    }

    // 3) Y = Kornaropoulos: topo order picking the root with the highest x_rank.
    out.y_rank.assign(n, 0);
    std::vector<uint32_t> in_deg(n, 0);
    for (uint32_t u = 0; u < n; ++u)
        for (size_t e = begin[u]; e < begin[u + 1]; ++e)
            ++in_deg[adj[e]];

    std::priority_queue<std::pair<uint32_t, uint32_t>> heap; // (x_rank, vertex)
    for (uint32_t v = 0; v < n; ++v)
        if (in_deg[v] == 0) heap.push({out.x_rank[v], v});

    uint32_t rank = 0;
    while (!heap.empty()) {
        uint32_t u = heap.top().second;
        heap.pop();
        out.y_rank[u] = rank++;
        for (size_t e = begin[u]; e < begin[u + 1]; ++e) {
            uint32_t w = adj[e];
            if (--in_deg[w] == 0) heap.push({out.x_rank[w], w});
        }
    }

    return out;
}

} // namespace feline
