#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace feline {

using vertex_t = uint32_t;
using edge_t   = uint64_t;

constexpr vertex_t INVALID_VERTEX = UINT32_MAX;

struct CSRGraph {
    uint32_t n = 0;
    uint64_t m = 0;
    std::vector<edge_t>   out_begin; // size n+1
    std::vector<vertex_t> out_adj;   // size m
};

struct CondensationResult {
    CSRGraph dag;
    std::vector<vertex_t> comp_id; // comp_id[v] = SCC id of original vertex v
    uint32_t num_components = 0;
};

// Load a directed graph from an edge-list file.
// Format: first line "n m", then m lines "u v" (0-indexed).
CSRGraph load_graph(const std::string& filename);

// Compute SCCs via iterative Tarjan and build the condensed DAG.
// If the graph is already a DAG, returns it as-is with identity mapping.
CondensationResult condense(const CSRGraph& g);

} // namespace feline
