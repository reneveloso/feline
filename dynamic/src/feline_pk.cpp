#include "feline_dyn/feline_pk.hpp"

#include "feline_dyn/dyn_query.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {
using namespace feline_dyn;

enum class Coord { X, Y };

static inline uint32_t coord(const DynIndex& idx, Coord c, vertex_t r) {
    return c == Coord::X ? idx.x(r) : idx.y(r);
}

// δ on coordinate `c`: the bidirectional fixpoint closure of {a, b} bounded by the
// coordinate band (coord(b), coord(a)). Starting from {a, b}, repeatedly pull in any
// predecessor p of an included vertex with coord(p) > coord(b), and any successor s
// of an included vertex with coord(s) < coord(a), until no more vertices qualify.
//
// A single "ancestors of a" ∪ "descendants of b" closure (filtered by position) is
// NOT sufficient: an ancestor of `a` can have an edge to a vertex w that is neither
// an ancestor of `a` nor a descendant of `b`, yet whose old coordinate still lies
// inside the band. Excluding w leaves it fixed while its neighbor gets reassigned to
// a new slot inside the band, which can invert their relative order and break that
// live edge. Confirmed by a random-DAG stress test: inserting edge (13,15) where
// y(13) > y(15) triggers a Y-reorder; vertex 10 is an ancestor of 13 (so it enters
// the naive δ) with a side edge 10->6; vertex 6 is unrelated to both 13 and 15 but
// sits at y=9, inside the (y(15)=0, y(13)=14) band. The naive δ excludes 6, and the
// reorder moves 10 to y=10 > 9, breaking the live edge 10->6. Closing δ under BOTH
// predecessor and successor edges (not just "ancestor of a" / "descendant of b") of
// every member, bounded by the band, pulls 6 in too and fixes this.
static std::vector<vertex_t> build_delta(const DynamicGraph& g, const DynIndex& idx,
                                         Coord c, vertex_t a, vertex_t b) {
    std::unordered_set<vertex_t> delta_set;
    std::vector<vertex_t> stk;
    uint32_t lo = coord(idx, c, b), hi = coord(idx, c, a);
    delta_set.insert(a); stk.push_back(a);
    delta_set.insert(b); stk.push_back(b);
    while (!stk.empty()) {
        vertex_t w = stk.back(); stk.pop_back();
        for (vertex_t p : g.dag_pred(w))
            if (coord(idx, c, p) > lo && delta_set.insert(p).second) stk.push_back(p);
        for (vertex_t s : g.dag_succ(w))
            if (coord(idx, c, s) < hi && delta_set.insert(s).second) stk.push_back(s);
    }
    return std::vector<vertex_t>(delta_set.begin(), delta_set.end());
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
static void reorder_acyclic(DynamicGraph& g, DynIndex& idx, vertex_t a, vertex_t b) {
    // A reorder is only needed on an axis where the new edge violates the current
    // order, i.e., x(a) > x(b) or y(a) > y(b). If both already hold (a before b on
    // both axes), just add the DAG edge.
    bool x_bad = idx.x(a) > idx.x(b);
    bool y_bad = idx.y(a) > idx.y(b);
    g.dag_add_edge(a, b);
    if (!x_bad && !y_bad) return;

    if (x_bad) {
        std::vector<vertex_t> delta_x = build_delta(g, idx, Coord::X, a, b);
        feline::XYOrdering sub = build_suborder(g, delta_x);
        sub.y_rank = identity_rank(idx, Coord::Y, delta_x); // leave Y untouched
        idx.permute(delta_x, sub);
    }
    if (y_bad) {
        std::vector<vertex_t> delta_y = build_delta(g, idx, Coord::Y, a, b);
        feline::XYOrdering sub = build_suborder(g, delta_y);
        sub.x_rank = identity_rank(idx, Coord::X, delta_y); // leave X untouched
        idx.permute(delta_y, sub);
    }
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

    // Remove all DAG edges touching folded reps, and drop folded reps from the DAG/index
    // (except r'). Collect edges first to avoid mutating while iterating.
    std::vector<std::pair<vertex_t, vertex_t>> to_remove;
    for (vertex_t c : cyc) {
        for (vertex_t s : g_.dag_succ(c)) to_remove.push_back({c, s});
        for (vertex_t p : g_.dag_pred(c)) to_remove.push_back({p, c});
    }
    for (auto [a, b] : to_remove) g_.dag_remove_edge(a, b);

    // Union r: all folded reps map to r'.
    std::vector<vertex_t> members(cyc.begin(), cyc.end());
    r_.unite(members, rprime);

    // Remove folded reps (except r') from the index and DAG vertex set.
    for (vertex_t c : cyc) {
        if (c == rprime) continue;
        if (idx_.has(c)) idx_.remove(c);
        g_.dag_remove_vertex(c);
    }
    g_.dag_add_vertex(rprime);

    // Rewire E_DAG: I -> r' and r' -> O (skip self-loops).
    for (vertex_t p : I) if (p != rprime) g_.dag_add_edge(p, rprime);
    for (vertex_t s : O) if (s != rprime) g_.dag_add_edge(rprime, s);

    // First-increment simplification: rebuild the whole index from the current DAG.
    std::vector<vertex_t> reps;
    reps.reserve(g_.dag_out_all().size());
    for (const auto& kv : g_.dag_out_all()) reps.push_back(kv.first);
    feline::XYOrdering ord = build_suborder(g_, reps);
    std::vector<vertex_t> ox(reps.size()), oy(reps.size());
    for (uint32_t i = 0; i < reps.size(); ++i) {
        ox[ord.x_rank[i]] = reps[i];
        oy[ord.y_rank[i]] = reps[i];
    }
    idx_.set_from_scratch(ox, oy);
}

} // namespace feline_dyn
