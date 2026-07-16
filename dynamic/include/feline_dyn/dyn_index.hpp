#pragma once

#include "feline/ordering.hpp"
#include "feline_dyn/dyn_graph.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace feline_dyn {

// The dynamic index: two topological orderings (X, Y), stored as GAP-TOLERANT signed
// integer coordinates (the original 2014 integer-coordinate representation). The index
// maps VERTICES, not just DAG representatives: a folded member (see FelinePK::fold_cycle)
// keeps the coordinate it held before folding, retained and inert for as long as it is
// not a representative — reachability queries map through the union-find representative
// first, and reorders only walk E_DAG neighbours, which are always representatives — so
// that a later component split (see FelinePK::split_component) can re-elect it and resume
// from a coordinate that already exists instead of needing a fresh one. Only the RELATIVE
// order of coordinate values is meaningful; removals leave gaps (O(1)) instead of
// compacting a dense 0..k-1 permutation. No operation ever needs a value strictly between
// two existing ones: reorders only permute a set's own existing coordinate values among
// themselves, and append_isolated only mints new values at the extremes, so with int64_t
// there is no exhaustion and no relabeling is ever needed.
class DynIndex {
public:
    bool has(vertex_t r) const { return x_coord_.find(r) != x_coord_.end(); }
    int64_t x(vertex_t r) const { return x_coord_.at(r); }
    int64_t y(vertex_t r) const { return y_coord_.at(r); }
    // Number of vertices that have coordinates — NOT the number of representatives
    // (folded members keep theirs). For the representative count use
    // graph().dag_out_all().size().
    uint32_t size() const { return static_cast<uint32_t>(x_coord_.size()); }

    // Alg. 7: append at the end of X and the front of Y (bottom-right corner). O(1).
    void append_isolated(vertex_t r);
    // Alg. 8: remove r from both orders. O(1) — leaves a gap, no compaction.
    void remove(vertex_t r);

    // Rebuild all coordinates from two topological orders of the current reps.
    void set_from_scratch(const std::vector<vertex_t>& order_x,
                          const std::vector<vertex_t>& order_y);

    // Reposition only the vertices in `delta` using their previous coordinate VALUES
    // as the slot set, ordered by the new relative ranks in `sub` (indexed positionally
    // by `delta`). Vertices outside `delta` keep their coordinates.
    void permute(const std::vector<vertex_t>& delta, const feline::XYOrdering& sub);

private:
    std::unordered_map<vertex_t, int64_t> x_coord_, y_coord_; // rep -> gap-tolerant coord
    int64_t next_x_ = 0; // next X coord to hand out at the high end (increments)
    int64_t next_y_ = 0; // next Y coord to hand out at the low end (decrements)
};

// Algorithm 6: build (X, Y) for the sub-DAG induced by `reps` (edges of E_DAG whose
// both endpoints are in `reps`). Ranks are returned positionally by `reps`.
feline::XYOrdering build_suborder(const DynamicGraph& g, const std::vector<vertex_t>& reps);

} // namespace feline_dyn
