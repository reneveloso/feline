#include "feline_dyn/dyn_graph.hpp"

#include <cassert>

namespace feline_dyn {

const VertexSet DynamicGraph::kEmpty{};

// ---- Representative (union-find) ----

void Representative::make_set(vertex_t v) {
    parent_[v] = v;
    rank_[v] = 0;
}

void Representative::erase(vertex_t v) {
    parent_.erase(v);
    rank_.erase(v);
}

bool Representative::contains(vertex_t v) const {
    return parent_.find(v) != parent_.end();
}

vertex_t Representative::find(vertex_t v) {
    assert(contains(v) && "Representative::find on unknown vertex");
    vertex_t root = v;
    while (parent_[root] != root) root = parent_[root];
    // path compression
    while (parent_[v] != root) {
        vertex_t next = parent_[v];
        parent_[v] = root;
        v = next;
    }
    return root;
}

vertex_t Representative::unite(const std::vector<vertex_t>& members, vertex_t chosen) {
    vertex_t root = find(chosen);
    for (vertex_t m : members) {
        vertex_t mroot = find(m);
        if (mroot == root) continue;
        parent_[mroot] = root;
        rank_[root] = rank_[root] > rank_[mroot] + 1 ? rank_[root] : rank_[mroot] + 1;
    }
    // `root` is now the root of the whole merged tree, but `chosen` may not
    // have been its own root at call time (root == find(chosen) could
    // differ from chosen). Re-point the tree so `chosen` is canonically the
    // representative: after this, find(chosen) == chosen unconditionally.
    if (root != chosen) {
        parent_[root] = chosen;
        rank_[chosen] = rank_[chosen] > rank_[root] + 1 ? rank_[chosen] : rank_[root] + 1;
        parent_[chosen] = chosen;
    }
    return chosen;
}

void Representative::repartition(const std::vector<std::vector<vertex_t>>& partitions) {
    // Reset every affected member first: a member of one partition may still be
    // pointed at by a member of another until all parents are cleared.
    for (const auto& part : partitions)
        for (vertex_t m : part) {
            parent_[m] = m;
            rank_[m] = 0;
        }
    for (const auto& part : partitions)
        if (!part.empty()) unite(part, part[0]);
}

// ---- DynamicGraph ----

void DynamicGraph::add_vertex(vertex_t v) {
    e_out_.try_emplace(v);
    e_in_.try_emplace(v);
}

void DynamicGraph::remove_vertex(vertex_t v) {
    e_out_.erase(v);
    e_in_.erase(v);
}

bool DynamicGraph::has_vertex(vertex_t v) const {
    return e_out_.find(v) != e_out_.end();
}

void DynamicGraph::add_edge(vertex_t u, vertex_t v) {
    e_out_[u].insert(v);
    e_in_[v].insert(u);
}

bool DynamicGraph::has_edge(vertex_t u, vertex_t v) const {
    auto it = e_out_.find(u);
    return it != e_out_.end() && it->second.count(v) > 0;
}

void DynamicGraph::remove_edge(vertex_t u, vertex_t v) {
    auto iu = e_out_.find(u);
    if (iu != e_out_.end()) iu->second.erase(v);
    auto iv = e_in_.find(v);
    if (iv != e_in_.end()) iv->second.erase(u);
}

const VertexSet& DynamicGraph::succ(vertex_t v) const {
    auto it = e_out_.find(v);
    return it == e_out_.end() ? kEmpty : it->second;
}

const VertexSet& DynamicGraph::pred(vertex_t v) const {
    auto it = e_in_.find(v);
    return it == e_in_.end() ? kEmpty : it->second;
}

void DynamicGraph::dag_add_vertex(vertex_t a) {
    dag_out_.try_emplace(a);
    dag_in_.try_emplace(a);
}

void DynamicGraph::dag_remove_vertex(vertex_t a) {
    dag_out_.erase(a);
    dag_in_.erase(a);
}

void DynamicGraph::dag_add_edge(vertex_t a, vertex_t b) {
    dag_out_[a].insert(b);
    dag_in_[b].insert(a);
}

void DynamicGraph::dag_remove_edge(vertex_t a, vertex_t b) {
    auto ia = dag_out_.find(a);
    if (ia != dag_out_.end()) ia->second.erase(b);
    auto ib = dag_in_.find(b);
    if (ib != dag_in_.end()) ib->second.erase(a);
}

const VertexSet& DynamicGraph::dag_succ(vertex_t a) const {
    auto it = dag_out_.find(a);
    return it == dag_out_.end() ? kEmpty : it->second;
}

const VertexSet& DynamicGraph::dag_pred(vertex_t a) const {
    auto it = dag_in_.find(a);
    return it == dag_in_.end() ? kEmpty : it->second;
}

} // namespace feline_dyn
