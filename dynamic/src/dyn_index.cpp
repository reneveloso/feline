#include "feline_dyn/dyn_index.hpp"

#include <algorithm>

namespace feline_dyn {

void DynIndex::append_isolated(vertex_t r) {
    // End of X (current high end), front of Y (current low end). O(1), no shifting.
    x_coord_[r] = next_x_++;
    y_coord_[r] = --next_y_;
}

void DynIndex::remove(vertex_t r) {
    // O(1): drop both coordinates, leaving a gap in each order. No compaction.
    x_coord_.erase(r);
    y_coord_.erase(r);
}

void DynIndex::remove_many(const std::vector<vertex_t>& reps) {
    // O(|reps|): each erase leaves a gap; survivors keep their coordinate values
    // and therefore their relative order on both axes.
    for (vertex_t r : reps) {
        x_coord_.erase(r);
        y_coord_.erase(r);
    }
}

void DynIndex::set_from_scratch(const std::vector<vertex_t>& order_x,
                                const std::vector<vertex_t>& order_y) {
    x_coord_.clear();
    y_coord_.clear();
    for (uint32_t i = 0; i < order_x.size(); ++i)
        x_coord_[order_x[i]] = static_cast<int64_t>(i);
    for (uint32_t i = 0; i < order_y.size(); ++i)
        y_coord_[order_y[i]] = static_cast<int64_t>(i);
    // After assigning X in [0, k-1], a subsequent append_isolated must go ABOVE the
    // current max (k-1): next_x_ = k gives next_x_++ == k. Y is assigned in [0, k-1]
    // too, and a subsequent append must go to the FRONT of Y (BELOW the current min,
    // 0): next_y_ = 0 gives --next_y_ == -1 < 0. Both extremes preserved.
    next_x_ = static_cast<int64_t>(order_x.size());
    next_y_ = 0;
}

void DynIndex::permute(const std::vector<vertex_t>& delta,
                       const feline::XYOrdering& sub) {
    // Slots = coordinate VALUES currently held by delta vertices, sorted ascending
    // (X and Y independently). The permute reassigns delta to exactly these values.
    std::vector<int64_t> xs, ys;
    xs.reserve(delta.size());
    ys.reserve(delta.size());
    for (vertex_t r : delta) {
        xs.push_back(x_coord_.at(r));
        ys.push_back(y_coord_.at(r));
    }
    std::sort(xs.begin(), xs.end());
    std::sort(ys.begin(), ys.end());

    // Order delta indices by the new relative ranks (sub is indexed positionally by delta).
    std::vector<uint32_t> by_x(delta.size()), by_y(delta.size());
    for (uint32_t i = 0; i < delta.size(); ++i) { by_x[i] = i; by_y[i] = i; }
    std::sort(by_x.begin(), by_x.end(),
              [&](uint32_t a, uint32_t b) { return sub.x_rank[a] < sub.x_rank[b]; });
    std::sort(by_y.begin(), by_y.end(),
              [&](uint32_t a, uint32_t b) { return sub.y_rank[a] < sub.y_rank[b]; });

    // Assign delta vertices to the sorted slot values in the new order.
    for (uint32_t k = 0; k < delta.size(); ++k) {
        x_coord_[delta[by_x[k]]] = xs[k];
        y_coord_[delta[by_y[k]]] = ys[k];
    }
}

feline::XYOrdering build_suborder(const DynamicGraph& g,
                                  const std::vector<vertex_t>& reps) {
    // Map representative id -> local index 0..k-1.
    std::unordered_map<vertex_t, uint32_t> local;
    local.reserve(reps.size());
    for (uint32_t i = 0; i < reps.size(); ++i) local[reps[i]] = i;

    feline::XYOrdering ord = feline::compute_xy_ordering(
        static_cast<uint32_t>(reps.size()),
        [&](uint32_t li, const auto& emit) {
            vertex_t r = reps[li];
            for (vertex_t w : g.dag_succ(r)) {
                auto it = local.find(w);
                if (it != local.end()) emit(it->second); // only edges within reps
            }
        });
    return ord; // ranks positional by reps
}

} // namespace feline_dyn
