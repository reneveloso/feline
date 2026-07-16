#pragma once

#include "feline_dyn/dyn_graph.hpp"
#include "feline_dyn/dyn_index.hpp"

#include <vector>

namespace feline_dyn {

// Feline-PK façade: maintains (DynamicGraph, Representative, DynIndex) under updates.
class FelinePK {
public:
    // Alg. 7: insert a disconnected vertex (O(1)).
    void insert_vertex(vertex_t v);
    // Alg. 8: remove a disconnected vertex (O(1)); v must be isolated in E.
    void remove_vertex(vertex_t v);

    // Alg. 10: insert edge (u, v). Defined in Tasks 8 (SCC unchanged) and 9 (SCC changed).
    void insert_edge(vertex_t u, vertex_t v);

    // Remove edge (u, v) from the digraph, updating r, E_DAG and the index. If (u,v) is
    // internal to a component, the component may split. No-op if the edge is absent.
    void remove_edge(vertex_t u, vertex_t v);

    // Dynamic reachability query (defined in Task 7).
    bool reachable(vertex_t u, vertex_t v);

    const DynamicGraph& graph() const { return g_; }
    Representative& rep() { return r_; }
    const DynIndex& index() const { return idx_; }

private:
    void fold_cycle(vertex_t u, vertex_t v, const std::vector<vertex_t>& v_cycles);
    void reinsert_dag_edge(vertex_t u, vertex_t v);
    void split_component(vertex_t old_rep, vertex_t u, vertex_t v);

    DynamicGraph g_;
    Representative r_;
    DynIndex idx_;
};

} // namespace feline_dyn
