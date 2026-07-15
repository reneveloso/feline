#include "test_util.hpp"
#include "feline_dyn/dyn_graph.hpp"
#include "feline_dyn/dyn_index.hpp"
#include "feline_dyn/dyn_query.hpp"
#include "feline_dyn/feline_pk.hpp"

void test_skeleton() {
    ASSERT(true, "skeleton builds and runs");
}

using namespace feline_dyn;

void test_union_find() {
    Representative r;
    r.make_set(0); r.make_set(1); r.make_set(2); r.make_set(3);
    ASSERT(r.find(0) == 0, "uf: singleton self");
    ASSERT(r.find(2) == 2, "uf: singleton self 2");
    r.unite({0, 1, 2}, 1);
    ASSERT(r.find(0) == 1, "uf: 0 -> 1");
    ASSERT(r.find(1) == 1, "uf: 1 -> 1");
    ASSERT(r.find(2) == 1, "uf: 2 -> 1");
    ASSERT(r.find(3) == 3, "uf: 3 untouched");
    ASSERT(r.contains(0) && !r.contains(9), "uf: contains");
}

void test_union_find_representative() {
    Representative r;
    r.make_set(0); r.make_set(1); r.make_set(2); r.make_set(3);

    vertex_t ret = r.unite({0, 1, 2}, 1);
    ASSERT(ret == 1, "uf-rep: unite returns chosen (1)");
    ASSERT(r.find(1) == 1, "uf-rep: find(chosen) == chosen after unite({0,1,2},1)");

    // Chained case: unite the set rooted at 1 with {3}, choosing 3.
    // Even though 1 was the prior representative, chosen=3 must become
    // the representative of the merged set.
    vertex_t ret2 = r.unite({1, 3}, 3);
    ASSERT(ret2 == 3, "uf-rep: unite returns chosen (3)");
    ASSERT(r.find(3) == 3, "uf-rep: find(chosen) == chosen after unite({1,3},3)");
    ASSERT(r.find(0) == 3, "uf-rep: chained find(0) == 3");
    ASSERT(r.find(1) == 3, "uf-rep: chained find(1) == 3");
    ASSERT(r.find(2) == 3, "uf-rep: chained find(2) == 3");
    ASSERT(r.find(3) == 3, "uf-rep: chained find(3) == 3");

    // Stronger case: `chosen` is a NON-root member of an already-merged set
    // at call time (its find() != itself before unite is invoked). This is
    // the precondition under which the pre-fix `unite` actually breaks the
    // "chosen stays the representative id" contract: croot = find(chosen)
    // != chosen, and chosen is never reparented back to itself.
    r.make_set(4); r.make_set(5); r.make_set(6); r.make_set(7);
    r.unite({4, 5, 6}, 5);              // root becomes 5; 4 is a non-root member.
    ASSERT(r.find(4) == 5, "uf-rep: setup, 4 -> 5");
    vertex_t ret3 = r.unite({5, 7}, 4);  // chosen=4 was NOT its own root here.
    ASSERT(ret3 == 4, "uf-rep: unite returns chosen (4) even when chosen wasn't root");
    ASSERT(r.find(4) == 4, "uf-rep: find(chosen) == chosen when chosen wasn't root at call time");
    ASSERT(r.find(5) == 4, "uf-rep: non-root-chosen find(5) == 4");
    ASSERT(r.find(6) == 4, "uf-rep: non-root-chosen find(6) == 4");
    ASSERT(r.find(7) == 4, "uf-rep: non-root-chosen find(7) == 4");
}

void test_dynamic_graph_edges() {
    DynamicGraph g;
    for (vertex_t v = 0; v < 4; ++v) g.add_vertex(v);
    g.add_edge(0, 1);
    g.add_edge(0, 2);
    g.add_edge(1, 3);
    ASSERT(g.succ(0).count(1) && g.succ(0).count(2) && g.succ(0).size() == 2, "dg: succ(0)");
    ASSERT(g.pred(3).count(1) && g.pred(3).size() == 1, "dg: pred(3)");
    ASSERT(g.succ(3).empty(), "dg: succ(3) empty");

    g.dag_add_vertex(0); g.dag_add_vertex(1);
    g.dag_add_edge(0, 1);
    ASSERT(g.dag_succ(0).count(1) && g.dag_pred(1).count(0), "dg: dag edge");
    g.dag_remove_edge(0, 1);
    ASSERT(g.dag_succ(0).empty() && g.dag_pred(1).empty(), "dg: dag edge removed");
}

void test_dynindex_isolated() {
    DynIndex idx;
    idx.append_isolated(10);
    idx.append_isolated(20);
    // 10 appended first: x(10) < x(20); y is prepended so later appends get smaller y.
    ASSERT(idx.has(10) && idx.has(20), "dynidx: has");
    ASSERT(idx.x(10) < idx.x(20), "dynidx: x order by append");
    ASSERT(idx.y(20) < idx.y(10), "dynidx: y prepended");
    ASSERT(idx.size() == 2, "dynidx: size 2");
    idx.remove(10);
    ASSERT(!idx.has(10) && idx.size() == 1, "dynidx: remove");
    ASSERT(idx.x(20) == 0, "dynidx: compacted x");
}

void test_build_suborder_chain() {
    // sub-DAG: 0 -> 1 -> 2  (reps {0,1,2})
    DynamicGraph g;
    for (vertex_t v = 0; v < 3; ++v) g.dag_add_vertex(v);
    g.dag_add_edge(0, 1);
    g.dag_add_edge(1, 2);
    std::vector<vertex_t> reps = {0, 1, 2};
    feline::XYOrdering ord = build_suborder(g, reps);
    // topological property on X and Y for edges (0,1),(1,2)
    ASSERT(ord.x_rank[0] < ord.x_rank[1] && ord.x_rank[1] < ord.x_rank[2], "suborder: X topo");
    ASSERT(ord.y_rank[0] < ord.y_rank[1] && ord.y_rank[1] < ord.y_rank[2], "suborder: Y topo");
}

void test_insert_remove_vertex() {
    FelinePK pk;
    pk.insert_vertex(5);
    pk.insert_vertex(7);
    ASSERT(pk.index().has(5) && pk.index().has(7), "pk: vertices indexed");
    ASSERT(pk.rep().find(5) == 5 && pk.rep().find(7) == 7, "pk: self-representatives");
    // isolated vertex is at bottom-right: x is max, y is min among current
    ASSERT(pk.index().x(5) < pk.index().x(7), "pk: x append order");
    pk.remove_vertex(5);
    ASSERT(!pk.index().has(5) && !pk.rep().contains(5), "pk: vertex removed");
    ASSERT(pk.index().has(7), "pk: other vertex intact");
}

static DynIndex make_index_diamond(DynamicGraph& g) {
    // DAG: 0->1, 0->2, 1->3, 2->3.  A valid (X,Y): use build_suborder.
    for (vertex_t v = 0; v < 4; ++v) g.dag_add_vertex(v);
    g.dag_add_edge(0, 1); g.dag_add_edge(0, 2);
    g.dag_add_edge(1, 3); g.dag_add_edge(2, 3);
    std::vector<vertex_t> reps = {0, 1, 2, 3};
    feline::XYOrdering ord = build_suborder(g, reps);
    std::vector<vertex_t> ox(4), oy(4);
    for (uint32_t i = 0; i < 4; ++i) { ox[ord.x_rank[i]] = reps[i]; oy[ord.y_rank[i]] = reps[i]; }
    DynIndex idx;
    idx.set_from_scratch(ox, oy);
    return idx;
}

void test_dyn_query_diamond() {
    DynamicGraph g;
    DynIndex idx = make_index_diamond(g);
    ASSERT(dyn_reachable(g, idx, 0, 3), "q: 0->3");
    ASSERT(dyn_reachable(g, idx, 0, 1), "q: 0->1");
    ASSERT(!dyn_reachable(g, idx, 1, 2), "q: 1-/->2");
    ASSERT(!dyn_reachable(g, idx, 3, 0), "q: 3-/->0");
    ASSERT(dyn_reachable(g, idx, 2, 3), "q: 2->3");
    auto p = reduced_rectangle(g, idx, 0, 3);
    std::vector<vertex_t> expected_p03 = {0, 1, 2, 3};
    ASSERT(p == expected_p03, "q: P(0,3) == {0,1,2,3}");
    auto p1 = reduced_rectangle(g, idx, 0, 1);
    std::vector<vertex_t> expected_p01 = {0, 1};
    ASSERT(p1 == expected_p01, "q: P(0,1) == {0,1}");
    auto p2 = reduced_rectangle(g, idx, 1, 2);
    ASSERT(p2.empty(), "q: P(1,2) empty");
}

void test_felinepk_reachable_isolated() {
    FelinePK pk;
    pk.insert_vertex(5);
    pk.insert_vertex(7);
    ASSERT(pk.reachable(5, 5), "pk-reach: same representative is reachable (reflexive)");
    ASSERT(!pk.reachable(5, 7), "pk-reach: disconnected 5 -/-> 7");
    ASSERT(!pk.reachable(7, 5), "pk-reach: disconnected 7 -/-> 5");
    ASSERT(!pk.reachable(99, 5), "pk-reach: unknown source guard");
    ASSERT(!pk.reachable(5, 99), "pk-reach: unknown target guard");
}

int main() {
    std::fprintf(stderr, "\n=== Feline-PK Dynamic Tests ===\n\n");
    RUN_TEST(test_skeleton);
    RUN_TEST(test_union_find);
    RUN_TEST(test_union_find_representative);
    RUN_TEST(test_dynamic_graph_edges);
    RUN_TEST(test_dynindex_isolated);
    RUN_TEST(test_build_suborder_chain);
    RUN_TEST(test_insert_remove_vertex);
    RUN_TEST(test_dyn_query_diamond);
    RUN_TEST(test_felinepk_reachable_isolated);
    TEST_SUMMARY();
    return dyntest::tests_failed > 0 ? 1 : 0;
}
