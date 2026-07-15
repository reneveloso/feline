#include "feline_dyn/feline_pk.hpp"

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

} // namespace feline_dyn
