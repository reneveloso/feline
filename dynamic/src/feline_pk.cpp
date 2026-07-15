#include "feline_dyn/feline_pk.hpp"

#include "feline_dyn/dyn_query.hpp"

namespace feline_dyn {

void FelinePK::insert_vertex(vertex_t v) {
    // Alg. 7: add to V, r(v) = v, append end of X / front of Y.
    g_.add_vertex(v);
    g_.dag_add_vertex(v);
    r_.make_set(v);
    idx_.append_isolated(v);
}

void FelinePK::remove_vertex(vertex_t v) {
    // Alg. 8: reverse of Alg. 7 (v must be isolated in E and its own representative).
    idx_.remove(v);
    r_.erase(v);
    g_.dag_remove_vertex(v);
    g_.remove_vertex(v);
}

bool FelinePK::reachable(vertex_t u, vertex_t v) {
    if (!r_.contains(u) || !r_.contains(v)) return false;
    vertex_t a = r_.find(u), b = r_.find(v);
    if (a == b) return true; // same SCC
    return dyn_reachable(g_, idx_, a, b);
}

} // namespace feline_dyn
