#include "feline_dyn/dyn_scc.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace feline_dyn {

bool reachable_within(const DynamicGraph& g, const std::unordered_set<vertex_t>& members,
                      vertex_t u, vertex_t v) {
    if (u == v) return true;
    if (!members.count(u)) return false;

    std::unordered_set<vertex_t> visited{u};
    std::vector<vertex_t> stk{u};
    while (!stk.empty()) {
        vertex_t n = stk.back();
        stk.pop_back();
        for (vertex_t w : g.succ(n)) {
            if (w == v) return true;
            if (!members.count(w)) continue;   // never leave the set
            if (visited.insert(w).second) stk.push_back(w);
        }
    }
    return false;
}

std::vector<std::vector<vertex_t>> tarjan_within(const DynamicGraph& g,
                                                 const std::vector<vertex_t>& members) {
    const std::unordered_set<vertex_t> in_set(members.begin(), members.end());

    std::unordered_map<vertex_t, uint32_t> index_of, lowlink;
    std::unordered_set<vertex_t> on_stack;
    std::vector<vertex_t> scc_stack;
    std::vector<std::vector<vertex_t>> out;
    uint32_t next_index = 0;

    // Explicit call stack: (vertex, its in-set successors, cursor into them).
    struct Frame {
        vertex_t v;
        std::vector<vertex_t> succs;
        size_t i;
    };

    auto in_set_succs = [&](vertex_t v) {
        std::vector<vertex_t> s;
        for (vertex_t w : g.succ(v))
            if (in_set.count(w)) s.push_back(w);
        return s;
    };

    for (vertex_t root : members) {
        if (index_of.count(root)) continue;

        std::vector<Frame> call;
        index_of[root] = lowlink[root] = next_index++;
        scc_stack.push_back(root);
        on_stack.insert(root);
        call.push_back({root, in_set_succs(root), 0});

        while (!call.empty()) {
            Frame& f = call.back();
            if (f.i < f.succs.size()) {
                vertex_t w = f.succs[f.i++];
                if (!index_of.count(w)) {
                    index_of[w] = lowlink[w] = next_index++;
                    scc_stack.push_back(w);
                    on_stack.insert(w);
                    call.push_back({w, in_set_succs(w), 0});
                } else if (on_stack.count(w)) {
                    lowlink[f.v] = std::min(lowlink[f.v], index_of[w]);
                }
            } else {
                const vertex_t v = f.v;
                if (lowlink[v] == index_of[v]) {
                    std::vector<vertex_t> comp;
                    while (true) {
                        vertex_t w = scc_stack.back();
                        scc_stack.pop_back();
                        on_stack.erase(w);
                        comp.push_back(w);
                        if (w == v) break;
                    }
                    out.push_back(std::move(comp));
                }
                const uint32_t v_low = lowlink[v];
                call.pop_back();
                if (!call.empty()) {
                    vertex_t parent = call.back().v;
                    lowlink[parent] = std::min(lowlink[parent], v_low);
                }
            }
        }
    }
    return out;
}

} // namespace feline_dyn
