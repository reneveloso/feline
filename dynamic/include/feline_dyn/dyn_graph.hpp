#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace feline_dyn {

using vertex_t = uint32_t;
using VertexSet = std::unordered_set<vertex_t>;

// Union-find over vertex ids: r(v) = SCC representative of v.
class Representative {
public:
    void make_set(vertex_t v);
    void erase(vertex_t v);              // only valid for a singleton set
    bool contains(vertex_t v) const;
    vertex_t find(vertex_t v);           // with path compression; asserts v is known (debug builds)
    // Union every member into `chosen`; returns `chosen`. All members must exist.
    // Postcondition: find(chosen) == chosen, unconditionally (even if `chosen`
    // was not its own root at call time) — `chosen` is guaranteed to be the
    // representative id of the merged set.
    vertex_t unite(const std::vector<vertex_t>& members, vertex_t chosen);

private:
    std::unordered_map<vertex_t, vertex_t> parent_;
    std::unordered_map<vertex_t, uint32_t> rank_;
};

// Mutable digraph (E) plus its associated DAG (E_DAG), each with forward and
// inverted adjacency, all stored as hashmaps (thesis Section 4.1).
class DynamicGraph {
public:
    // --- digraph E ---
    void add_vertex(vertex_t v);
    void remove_vertex(vertex_t v);      // v must be isolated in E
    bool has_vertex(vertex_t v) const;
    void add_edge(vertex_t u, vertex_t v);
    bool has_edge(vertex_t u, vertex_t v) const;
    void remove_edge(vertex_t u, vertex_t v);   // no-op if the edge is absent
    const std::unordered_map<vertex_t, VertexSet>& out_all() const { return e_out_; }
    const VertexSet& succ(vertex_t v) const;
    const VertexSet& pred(vertex_t v) const;

    // --- associated DAG E_DAG (keys are representatives) ---
    void dag_add_vertex(vertex_t a);
    void dag_remove_vertex(vertex_t a);
    void dag_add_edge(vertex_t a, vertex_t b);
    void dag_remove_edge(vertex_t a, vertex_t b);
    const VertexSet& dag_succ(vertex_t a) const;
    const VertexSet& dag_pred(vertex_t a) const;
    const std::unordered_map<vertex_t, VertexSet>& dag_out_all() const { return dag_out_; }

private:
    std::unordered_map<vertex_t, VertexSet> e_out_, e_in_;
    std::unordered_map<vertex_t, VertexSet> dag_out_, dag_in_;
    static const VertexSet kEmpty;
};

} // namespace feline_dyn
