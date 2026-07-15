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
    RUN_TEST(test_dynamic_graph_edges);
    TEST_SUMMARY();
    return dyntest::tests_failed > 0 ? 1 : 0;
}
