#include "feline_dyn/feline_pk.hpp"

#include "feline_dyn/dyn_query.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {
using namespace feline_dyn;

enum class Coord { X, Y };

static inline int64_t coord(const DynIndex& idx, Coord c, vertex_t r) {
    return c == Coord::X ? idx.x(r) : idx.y(r);
}

// Naive δ on coordinate `c`, per the thesis's original reorder scheme (no bidirectional
// closure). Two flavors, selected by `follow_pred`:
//   - Sig_B (follow_pred = true): {a} ∪ ancestors of `a` (via dag_pred) whose coord on
//     `c` is > lo. DFS from `start` = a, recursing into a predecessor p only if
//     coord(p) > lo.
//   - Sig_F (follow_pred = false): {b} ∪ descendants of `b` (via dag_succ) whose coord
//     on `c` is < hi. DFS from `start` = b, recursing into a successor s only if
//     coord(s) < hi.
// Explicit stack, no recursion; visited set dedupes.
static std::vector<vertex_t> collect_in_band(const DynamicGraph& g, const DynIndex& idx,
                                             Coord c, vertex_t start, int64_t lo,
                                             int64_t hi, bool follow_pred) {
    std::unordered_set<vertex_t> visited;
    std::vector<vertex_t> out;
    std::vector<vertex_t> stk;
    visited.insert(start);
    stk.push_back(start);
    while (!stk.empty()) {
        vertex_t w = stk.back(); stk.pop_back();
        out.push_back(w);
        if (follow_pred) {
            for (vertex_t p : g.dag_pred(w))
                if (coord(idx, c, p) > lo && visited.insert(p).second) stk.push_back(p);
        } else {
            for (vertex_t s : g.dag_succ(w))
                if (coord(idx, c, s) < hi && visited.insert(s).second) stk.push_back(s);
        }
    }
    return out;
}

// Structured merge rank on coordinate `c`: every Sig_B vertex ranks below every Sig_F
// vertex; within each group, ascending by current coord(idx, c, ·). Combined with
// DynIndex::permute (which reassigns δ to the sorted list of its OWN old slots on `c`,
// in this rank order), this sends δ_in = Sig_B to the lowest slots and δ_out = Sig_F to
// the highest slots, each preserving its members' relative old-coordinate order. That
// structure is what makes the naive (non-closure) δ safe: every δ_in vertex's slot only
// decreases and every δ_out vertex's slot only increases, so an edge from a δ_in vertex
// to a vertex OUTSIDE δ (which keeps its old slot, inside or above the band) can't be
// inverted, and symmetrically for δ_out -> outside edges.
static std::vector<uint32_t> merge_rank(const DynIndex& idx, Coord c,
                                        const std::vector<vertex_t>& delta,
                                        const std::unordered_set<vertex_t>& sigB) {
    std::vector<uint32_t> order(delta.size());
    for (uint32_t i = 0; i < delta.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](uint32_t p, uint32_t q) {
        bool fp = !sigB.count(delta[p]); // false (0) = Sig_B, true (1) = Sig_F
        bool fq = !sigB.count(delta[q]);
        if (fp != fq) return fp < fq;
        return coord(idx, c, delta[p]) < coord(idx, c, delta[q]);
    });
    std::vector<uint32_t> rank(delta.size());
    for (uint32_t k = 0; k < order.size(); ++k) rank[order[k]] = k;
    return rank;
}

} // namespace

namespace feline_dyn {

// Rank of each `delta[i]` by its OWN current position on coordinate `c`, ascending.
// Feeding this back as the "new" rank for that axis into DynIndex::permute is a
// no-op: permute reassigns delta to the sorted list of its own old slots on that
// axis, in this exact order, which reproduces each vertex's original position.
static std::vector<uint32_t> identity_rank(const DynIndex& idx, Coord c,
                                           const std::vector<vertex_t>& delta) {
    std::vector<uint32_t> order(delta.size());
    for (uint32_t i = 0; i < delta.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](uint32_t p, uint32_t q) {
        return coord(idx, c, delta[p]) < coord(idx, c, delta[q]);
    });
    std::vector<uint32_t> rank(delta.size());
    for (uint32_t k = 0; k < order.size(); ++k) rank[order[k]] = k;
    return rank;
}

// Builds δ and reorders it. Precondition: a = r(u), b = r(v), a != b, edge does not
// create a cycle (b does not reach a). Adds (a,b) to E_DAG and permutes δ in X and Y.
//
// Thesis-faithful reorder: for each violated axis, δ is the NAIVE union of δ_in =
// Sig_B = {a} ∪ ancestors-of-a in the (coord(b), coord(a)] band and δ_out = Sig_F =
// {b} ∪ descendants-of-b in the [coord(b), coord(a)) band — no bidirectional closure.
// The merge-rank sends every δ_in vertex to the lowest slots of the band and every
// δ_out vertex to the highest, each group preserving its members' old relative order
// (see merge_rank's comment for why this makes the naive δ safe).
//
// X and Y are reordered in two INDEPENDENT phases, each with its own δ filtered on
// a single coordinate. Combining both coordinates into one shared δ (filtered by
// "x(w) > x(b) || y(w) > y(b)") and permuting X and Y together from that shared δ is
// unsound: a vertex can enter δ solely because it violates Y while its X position is
// unrelated to the fix, yet DynIndex::permute() would still reuse its X slot as part
// of the shared pool — corrupting X order for an unrelated live edge whose endpoint
// happens to sit inside that slot range (confirmed by a random-DAG stress test: with
// edge (11,7) where x(11)<x(7) already (x_bad=false) but y(11)>y(7) (y_bad=true), a
// combined δ still moved vertex 8 to a smaller X slot than vertex 5, breaking the
// live edge 5->8 on X even though X was never violated). Splitting into two
// single-axis phases — each leaving the other axis untouched via identity_rank() —
// keeps every δ within the classical bounded ancestor/descendant band per axis.
static void reorder_axis(DynamicGraph& g, DynIndex& idx, Coord c, vertex_t a, vertex_t b) {
    int64_t lo = coord(idx, c, b), hi = coord(idx, c, a);
    std::vector<vertex_t> sig_b = collect_in_band(g, idx, c, a, lo, hi, /*follow_pred=*/true);
    std::vector<vertex_t> sig_f = collect_in_band(g, idx, c, b, lo, hi, /*follow_pred=*/false);

    std::unordered_set<vertex_t> sigB_membership(sig_b.begin(), sig_b.end());
    std::vector<vertex_t> delta = sig_b;
    for (vertex_t w : sig_f)
        if (!sigB_membership.count(w)) delta.push_back(w); // dedupe: keep as Sig_B if overlap

    feline::XYOrdering sub;
    if (c == Coord::X) {
        sub.x_rank = merge_rank(idx, Coord::X, delta, sigB_membership);
        sub.y_rank = identity_rank(idx, Coord::Y, delta); // leave Y untouched
    } else {
        sub.y_rank = merge_rank(idx, Coord::Y, delta, sigB_membership);
        sub.x_rank = identity_rank(idx, Coord::X, delta); // leave X untouched
    }
    idx.permute(delta, sub);
}

// Reorder for an edge a->b that ALREADY exists in E_DAG. Reorders each axis where
// the edge violates the current order (x(a) > x(b) or y(a) > y(b)); leaves a
// satisfied axis untouched. Does NOT add the edge — the caller guarantees it exists.
static void reorder_for_edge(DynamicGraph& g, DynIndex& idx, vertex_t a, vertex_t b) {
    bool x_bad = idx.x(a) > idx.x(b);
    bool y_bad = idx.y(a) > idx.y(b);
    if (x_bad) reorder_axis(g, idx, Coord::X, a, b);
    if (y_bad) reorder_axis(g, idx, Coord::Y, a, b);
}

static void reorder_acyclic(DynamicGraph& g, DynIndex& idx, vertex_t a, vertex_t b) {
    // A reorder is only needed on an axis where the new edge violates the current
    // order, i.e., x(a) > x(b) or y(a) > y(b). If both already hold (a before b on
    // both axes), reorder_for_edge is a no-op. Add the DAG edge, then reorder.
    g.dag_add_edge(a, b);
    reorder_for_edge(g, idx, a, b);
}

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

void FelinePK::insert_edge(vertex_t u, vertex_t v) {
    // Endpoints are auto-created if absent, so edges are self-bootstrapping.
    if (!r_.contains(u)) insert_vertex(u);
    if (!r_.contains(v)) insert_vertex(v);
    g_.add_edge(u, v);
    vertex_t a = r_.find(u), b = r_.find(v);
    if (a == b) return; // internal SCC edge

    // Does adding a->b close a cycle? Only if b already reaches a in E_DAG.
    std::vector<vertex_t> v_cycles = reduced_rectangle(g_, idx_, b, a);
    if (v_cycles.empty()) {
        reorder_acyclic(g_, idx_, a, b); // SCC unchanged
    } else {
        fold_cycle(u, v, v_cycles);      // SCC changed (Task 9)
    }
}

void FelinePK::fold_cycle(vertex_t u, vertex_t v, const std::vector<vertex_t>& v_cycles) {
    // Elect representative r' deterministically: r(v).
    vertex_t rprime = r_.find(v);

    std::unordered_set<vertex_t> cyc(v_cycles.begin(), v_cycles.end());
    cyc.insert(r_.find(u)); // ensure both endpoints' reps are in the folded set
    cyc.insert(r_.find(v));

    // I = DAG predecessors of the set outside it; O = DAG successors outside it.
    std::unordered_set<vertex_t> I, O;
    for (vertex_t c : cyc) {
        for (vertex_t p : g_.dag_pred(c)) if (!cyc.count(p)) I.insert(p);
        for (vertex_t s : g_.dag_succ(c)) if (!cyc.count(s)) O.insert(s);
    }
    // SCC-convexity invariant: I and O must be disjoint, or the contracted DAG
    // (I -> r' -> O) would contain a cycle and break build_suborder's DAG assumption.
    for (vertex_t x : I) assert(!O.count(x) && "fold_cycle: I and O overlap => broken reachable=>dominates invariant");

    // Remove all DAG edges touching folded reps. Collect edges first to avoid
    // mutating while iterating.
    std::vector<std::pair<vertex_t, vertex_t>> to_remove;
    for (vertex_t c : cyc) {
        for (vertex_t s : g_.dag_succ(c)) to_remove.push_back({c, s});
        for (vertex_t p : g_.dag_pred(c)) to_remove.push_back({p, c});
    }
    for (auto [a, b] : to_remove) g_.dag_remove_edge(a, b);

    // Union r: all folded reps map to r'.
    std::vector<vertex_t> members(cyc.begin(), cyc.end());
    r_.unite(members, rprime);

    // Remove folded reps (except r') from the DAG vertex set. r' stays; the folded
    // members are absorbed via union-find (r.find() maps them to r').
    for (vertex_t c : cyc) {
        if (c == rprime) continue;
        g_.dag_remove_vertex(c);
    }
    g_.dag_add_vertex(rprime);

    // Rewire E_DAG: I -> r' and r' -> O (skip self-loops).
    for (vertex_t p : I) if (p != rprime) g_.dag_add_edge(p, rprime);
    for (vertex_t s : O) if (s != rprime) g_.dag_add_edge(rprime, s);

    // Localized SCC-fold reposition (Alg. 10 lines 21-22): move r' locally instead
    // of rebuilding the whole index.
    //
    // Folded members leave the DAG but KEEP their coordinates: if an edge removal later
    // splits this component, a re-elected representative resumes near its old position
    // instead of being re-inserted at an extreme corner. Their coordinates are inert
    // while they are not representatives (queries map through r.find() first, and
    // reorders only walk E_DAG neighbours, which are always representatives).

    // Reposition r' so every incident E_DAG edge is satisfied, reusing the
    // thesis-faithful acyclic reorder (reorder_for_edge) on the already-present
    // I -> r' and r' -> O edges. Snapshot I and O into local vectors so the
    // permute() calls (which never touch the edge sets) can't disturb iteration.
    std::vector<vertex_t> preds, succs;
    for (vertex_t p : I) if (p != rprime) preds.push_back(p);
    for (vertex_t s : O) if (s != rprime) succs.push_back(s);

    auto all_satisfied = [&]() {
        for (vertex_t p : preds)
            if (!(idx_.x(p) < idx_.x(rprime) && idx_.y(p) < idx_.y(rprime))) return false;
        for (vertex_t s : succs)
            if (!(idx_.x(rprime) < idx_.x(s) && idx_.y(rprime) < idx_.y(s))) return false;
        return true;
    };

    // A single pred-then-succ pass may leave a residual violation in rare
    // interleavings; repeat until r' has no violated incident edge (safety cap).
    const int kMaxPasses = 8;
    int pass = 0;
    for (; pass < kMaxPasses && !all_satisfied(); ++pass) {
        for (vertex_t p : preds) reorder_for_edge(g_, idx_, p, rprime);
        for (vertex_t s : succs) reorder_for_edge(g_, idx_, rprime, s);
    }
    assert(all_satisfied() && "fold_cycle: r' reposition hit safety cap without satisfying all incident edges");
}

} // namespace feline_dyn
