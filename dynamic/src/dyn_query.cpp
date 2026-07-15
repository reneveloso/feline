#include "feline_dyn/dyn_query.hpp"

#include <algorithm>
#include <unordered_set>

namespace feline_dyn {

// True iff a dominates b in the 2D plane (necessary condition for a ->* b).
static inline bool dominates(const DynIndex& idx, vertex_t a, vertex_t b) {
    return idx.x(a) <= idx.x(b) && idx.y(a) <= idx.y(b);
}

bool dyn_reachable(const DynamicGraph& g, const DynIndex& idx, vertex_t a, vertex_t b) {
    if (a == b) return true;
    if (!dominates(idx, a, b)) return false; // negative-cut

    // Iterative DFS over E_DAG, pruning any node that does not dominate b.
    std::vector<vertex_t> stk;
    std::unordered_set<vertex_t> visited;
    stk.push_back(a);
    visited.insert(a);
    while (!stk.empty()) {
        vertex_t node = stk.back();
        stk.pop_back();
        for (vertex_t w : g.dag_succ(node)) {
            if (w == b) return true;
            if (visited.count(w)) continue;
            if (!dominates(idx, w, b)) continue; // prune: w cannot reach b
            visited.insert(w);
            stk.push_back(w);
        }
    }
    return false;
}

std::vector<vertex_t> reduced_rectangle(const DynamicGraph& g, const DynIndex& idx,
                                        vertex_t a, vertex_t b) {
    std::vector<vertex_t> result;
    if (a == b) { result.push_back(a); return result; }
    if (!dominates(idx, a, b)) return result;

    // Forward DFS from a, keeping only nodes that dominate b; collect those that
    // actually reach b (i.e., lie on an a ->* b path).
    std::unordered_set<vertex_t> reaches_b; // memo of "can reach b"
    std::unordered_set<vertex_t> in_rect;   // nodes within the rectangle, visited
    std::vector<vertex_t> order;            // post-order for backward propagation

    std::vector<std::pair<vertex_t, bool>> stk; // (node, children_expanded)
    stk.push_back({a, false});
    while (!stk.empty()) {
        auto [node, expanded] = stk.back();
        stk.pop_back();
        if (!expanded) {
            if (in_rect.count(node)) continue;
            in_rect.insert(node);
            stk.push_back({node, true});
            for (vertex_t w : g.dag_succ(node)) {
                if (w == b) { reaches_b.insert(node); continue; }
                if (!dominates(idx, w, b)) continue;
                if (!in_rect.count(w)) stk.push_back({w, false});
            }
        } else {
            order.push_back(node);
        }
    }
    reaches_b.insert(b);
    // Propagate "reaches b" in reverse topological (post-order) order.
    for (vertex_t node : order) {
        if (reaches_b.count(node)) continue;
        for (vertex_t w : g.dag_succ(node)) {
            if (reaches_b.count(w)) { reaches_b.insert(node); break; }
        }
    }
    if (!reaches_b.count(a)) return result; // b not reachable from a

    for (vertex_t node : in_rect) {
        if (reaches_b.count(node)) result.push_back(node);
    }
    result.push_back(b);
    // Deduplicate (b may already be present if it was in in_rect).
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

} // namespace feline_dyn
