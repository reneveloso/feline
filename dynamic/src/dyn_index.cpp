#include "feline_dyn/dyn_index.hpp"

#include <algorithm>
#include <unordered_set>

namespace feline_dyn {

void DynIndex::reindex_from(const std::vector<vertex_t>& x_at,
                            const std::vector<vertex_t>& y_at) {
    x_at_ = x_at;
    y_at_ = y_at;
    x_pos_.clear();
    y_pos_.clear();
    for (uint32_t i = 0; i < x_at_.size(); ++i) x_pos_[x_at_[i]] = i;
    for (uint32_t i = 0; i < y_at_.size(); ++i) y_pos_[y_at_[i]] = i;
}

void DynIndex::append_isolated(vertex_t r) {
    // End of X.
    x_pos_[r] = static_cast<uint32_t>(x_at_.size());
    x_at_.push_back(r);
    // Front of Y: shift all existing Y positions up by one.
    y_at_.insert(y_at_.begin(), r);
    for (uint32_t i = 0; i < y_at_.size(); ++i) y_pos_[y_at_[i]] = i;
}

void DynIndex::remove(vertex_t r) {
    uint32_t xp = x_pos_.at(r), yp = y_pos_.at(r);
    x_at_.erase(x_at_.begin() + xp);
    y_at_.erase(y_at_.begin() + yp);
    x_pos_.erase(r);
    y_pos_.erase(r);
    for (uint32_t i = xp; i < x_at_.size(); ++i) x_pos_[x_at_[i]] = i;
    for (uint32_t i = yp; i < y_at_.size(); ++i) y_pos_[y_at_[i]] = i;
}

void DynIndex::remove_many(const std::vector<vertex_t>& reps) {
    std::unordered_set<vertex_t> drop(reps.begin(), reps.end());
    // Rebuild x_at_/y_at_ by copying over survivors in their current order.
    std::vector<vertex_t> new_x, new_y;
    new_x.reserve(x_at_.size());
    new_y.reserve(y_at_.size());
    for (vertex_t r : x_at_) if (!drop.count(r)) new_x.push_back(r);
    for (vertex_t r : y_at_) if (!drop.count(r)) new_y.push_back(r);
    reindex_from(new_x, new_y);
}

void DynIndex::set_from_scratch(const std::vector<vertex_t>& order_x,
                                const std::vector<vertex_t>& order_y) {
    reindex_from(order_x, order_y);
}

void DynIndex::permute(const std::vector<vertex_t>& delta,
                       const feline::XYOrdering& sub) {
    // Slots = positions currently held by delta vertices, sorted ascending (X and Y).
    std::vector<uint32_t> x_slots, y_slots;
    x_slots.reserve(delta.size());
    y_slots.reserve(delta.size());
    for (vertex_t r : delta) {
        x_slots.push_back(x_pos_.at(r));
        y_slots.push_back(y_pos_.at(r));
    }
    std::sort(x_slots.begin(), x_slots.end());
    std::sort(y_slots.begin(), y_slots.end());

    // Order delta by the new relative ranks (sub is indexed positionally by delta).
    std::vector<uint32_t> by_x(delta.size()), by_y(delta.size());
    for (uint32_t i = 0; i < delta.size(); ++i) { by_x[i] = i; by_y[i] = i; }
    std::sort(by_x.begin(), by_x.end(),
              [&](uint32_t a, uint32_t b) { return sub.x_rank[a] < sub.x_rank[b]; });
    std::sort(by_y.begin(), by_y.end(),
              [&](uint32_t a, uint32_t b) { return sub.y_rank[a] < sub.y_rank[b]; });

    // Assign delta vertices to the sorted slots in the new order.
    for (uint32_t k = 0; k < delta.size(); ++k) {
        vertex_t rx = delta[by_x[k]];
        x_at_[x_slots[k]] = rx;
        x_pos_[rx] = x_slots[k];
        vertex_t ry = delta[by_y[k]];
        y_at_[y_slots[k]] = ry;
        y_pos_[ry] = y_slots[k];
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
