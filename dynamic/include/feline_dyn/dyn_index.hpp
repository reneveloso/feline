#pragma once

#include "feline/ordering.hpp"
#include "feline_dyn/dyn_graph.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace feline_dyn {

// The dynamic index: two topological orderings (X, Y) over DAG representatives,
// stored as position maps plus their inverses for O(1) lookup and permutation.
class DynIndex {
public:
    bool has(vertex_t r) const { return x_pos_.find(r) != x_pos_.end(); }
    uint32_t x(vertex_t r) const { return x_pos_.at(r); }
    uint32_t y(vertex_t r) const { return y_pos_.at(r); }
    uint32_t size() const { return static_cast<uint32_t>(x_at_.size()); }

    // Alg. 7: append at the end of X and the front of Y (bottom-right corner).
    void append_isolated(vertex_t r);
    // Alg. 8: remove r from both orders, compacting positions.
    void remove(vertex_t r);

    // Remove a SET of reps from both orders in a single O(size) compaction pass
    // (as opposed to O(size) per removed rep). Survivors keep their relative
    // order; positions compact to 0..(size-|reps|-1).
    void remove_many(const std::vector<vertex_t>& reps);

    // Rebuild all positions from two topological orders of the current reps.
    void set_from_scratch(const std::vector<vertex_t>& order_x,
                          const std::vector<vertex_t>& order_y);

    // Reposition only the vertices in `delta` using their previous positions as the
    // slot set, ordered by the new relative ranks in `sub` (indexed positionally by
    // `delta`). Vertices outside `delta` keep their positions.
    void permute(const std::vector<vertex_t>& delta, const feline::XYOrdering& sub);

    const std::vector<vertex_t>& x_order() const { return x_at_; }
    const std::vector<vertex_t>& y_order() const { return y_at_; }

private:
    void reindex_from(const std::vector<vertex_t>& x_at, const std::vector<vertex_t>& y_at);
    std::unordered_map<vertex_t, uint32_t> x_pos_, y_pos_;
    std::vector<vertex_t> x_at_, y_at_; // position -> representative
};

// Algorithm 6: build (X, Y) for the sub-DAG induced by `reps` (edges of E_DAG whose
// both endpoints are in `reps`). Ranks are returned positionally by `reps`.
feline::XYOrdering build_suborder(const DynamicGraph& g, const std::vector<vertex_t>& reps);

} // namespace feline_dyn
