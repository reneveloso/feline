#pragma once

#include "feline_dyn/dyn_graph.hpp"

#include <unordered_set>
#include <vector>

namespace feline_dyn {

// Iterative Tarjan over the subgraph induced by `members`, following only E edges whose
// BOTH endpoints are in `members` (E ∩ C²). Returns the strongly connected partitions;
// every vertex of `members` appears in exactly one partition.
std::vector<std::vector<vertex_t>> tarjan_within(const DynamicGraph& g,
                                                 const std::vector<vertex_t>& members);

// Iterative DFS from u over E that never leaves `members`. True iff v is reachable from
// u without leaving the set. u == v returns true only when u is in `members` — both
// endpoints must be in the set.
bool reachable_within(const DynamicGraph& g, const std::unordered_set<vertex_t>& members,
                      vertex_t u, vertex_t v);

} // namespace feline_dyn
