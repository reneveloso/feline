#pragma once

#include "feline/graph.hpp"
#include <vector>
#include <string>

namespace feline {

struct FELINEIndex {
    std::vector<uint32_t> x_rank;         // X coordinate (topological order)
    std::vector<uint32_t> y_rank;         // Y coordinate (Kornaropoulos heuristic)
    std::vector<uint32_t> interval_start; // s_u (positive-cut, min-post)
    std::vector<uint32_t> interval_end;   // e_u (positive-cut, min-post)
    std::vector<uint32_t> level;          // level filter
};

// Build the complete FELINE index for a DAG.
FELINEIndex build_index(const CSRGraph& dag);

// Export the index to a CSV file: vertex,x,y,level
void export_index_csv(const FELINEIndex& idx, const std::string& filename);

} // namespace feline
