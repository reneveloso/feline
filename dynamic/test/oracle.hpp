#pragma once

#include "feline_dyn/dyn_graph.hpp"

#include <queue>
#include <unordered_set>
#include <vector>

namespace dyntest {

// Ground-truth BFS reachability over the digraph E of a DynamicGraph.
inline bool bfs_reachable(const feline_dyn::DynamicGraph& g,
                          feline_dyn::vertex_t u, feline_dyn::vertex_t v) {
    if (u == v) return true;
    std::unordered_set<feline_dyn::vertex_t> visited;
    std::queue<feline_dyn::vertex_t> q;
    visited.insert(u);
    q.push(u);
    while (!q.empty()) {
        feline_dyn::vertex_t cur = q.front();
        q.pop();
        for (feline_dyn::vertex_t w : g.succ(cur)) {
            if (w == v) return true;
            if (visited.insert(w).second) q.push(w);
        }
    }
    return false;
}

} // namespace dyntest
