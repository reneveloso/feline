#pragma once

#include "feline_dyn/dyn_graph.hpp"
#include "feline_dyn/dyn_index.hpp"

#include <vector>

namespace feline_dyn {

// Dominance rectangle query: is representative b reachable from representative a,
// using X/Y negative-cut pruning + DFS over E_DAG. a, b must be representatives.
bool dyn_reachable(const DynamicGraph& g, const DynIndex& idx, vertex_t a, vertex_t b);

// P(a, b): representatives on some a ->* b path that also fall inside the (a,b)
// dominance rectangle. Empty iff b is not reachable from a. Includes a and b otherwise.
std::vector<vertex_t> reduced_rectangle(const DynamicGraph& g, const DynIndex& idx,
                                        vertex_t a, vertex_t b);

} // namespace feline_dyn
