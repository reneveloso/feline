#include "test_util.hpp"
#include "feline_dyn/dyn_graph.hpp"

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

int main() {
    std::fprintf(stderr, "\n=== Feline-PK Dynamic Tests ===\n\n");
    RUN_TEST(test_skeleton);
    RUN_TEST(test_union_find);
    RUN_TEST(test_union_find_representative);
    RUN_TEST(test_dynamic_graph_edges);
    TEST_SUMMARY();
    return dyntest::tests_failed > 0 ? 1 : 0;
}
