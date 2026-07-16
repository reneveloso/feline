#include "test_util.hpp"
#include "oracle.hpp"
#include "feline/graph.hpp"
#include "feline_dyn/dyn_graph.hpp"
#include "feline_dyn/dyn_index.hpp"
#include "feline_dyn/dyn_query.hpp"
#include "feline_dyn/feline_pk.hpp"

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <random>
#include <string>
#include <vector>

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
    // Gap-tolerant coords: capture the survivor's coordinates before removal so we
    // can assert they are UNCHANGED by the O(1) remove (no compaction / relabeling).
    int64_t x20 = idx.x(20), y20 = idx.y(20);
    idx.remove(10);
    ASSERT(!idx.has(10) && idx.size() == 1, "dynidx: remove");
    ASSERT(idx.x(20) == x20 && idx.y(20) == y20, "dynidx: survivor coords unchanged (gap left, no compaction)");
    // Append order still holds against the survivor: new vertex goes to the high end
    // of X (x(30) > x(20)) and the front of Y (y(30) < y(20)).
    idx.append_isolated(30);
    ASSERT(idx.x(30) > idx.x(20), "dynidx: append at high end of X");
    ASSERT(idx.y(30) < idx.y(20), "dynidx: append at front of Y");
}

void test_dynindex_remove_many() {
    // Build an index over 6 reps via set_from_scratch, then bulk-remove a subset.
    DynIndex idx;
    // X order: 10,20,30,40,50,60 ; Y order: reversed to make positions distinct.
    std::vector<vertex_t> ox = {10, 20, 30, 40, 50, 60};
    std::vector<vertex_t> oy = {60, 50, 40, 30, 20, 10};
    idx.set_from_scratch(ox, oy);
    ASSERT(idx.size() == 6, "remove_many: initial size 6");

    // Remove {20, 40, 60}. Survivors in X order: 10,30,50 ; in Y order: 50,30,10.
    idx.remove_many({20, 40, 60});
    ASSERT(idx.size() == 3, "remove_many: size 3 after removing 3");
    ASSERT(!idx.has(20) && !idx.has(40) && !idx.has(60), "remove_many: removed reps gone");
    ASSERT(idx.has(10) && idx.has(30) && idx.has(50), "remove_many: survivors present");

    // Gap-tolerant coords: survivors KEEP their original coordinate values (no
    // compaction) and thus their relative order on each axis. X order 10<30<50 and
    // Y order 50<30<10 must still hold as strict inequalities.
    ASSERT(idx.x(10) < idx.x(30) && idx.x(30) < idx.x(50), "remove_many: X relative order preserved");
    ASSERT(idx.y(50) < idx.y(30) && idx.y(30) < idx.y(10), "remove_many: Y relative order preserved");
    // Values are exactly the originals from set_from_scratch (survivors untouched).
    ASSERT(idx.x(10) == 0 && idx.x(30) == 2 && idx.x(50) == 4, "remove_many: X survivor values unchanged");
    ASSERT(idx.y(50) == 1 && idx.y(30) == 3 && idx.y(10) == 5, "remove_many: Y survivor values unchanged");
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

// Insert a sequence of edges (no cycles) and verify all-pairs vs BFS oracle each step.
void test_insert_edge_acyclic_oracle() {
    FelinePK pk;
    const vertex_t N = 6;
    for (vertex_t v = 0; v < N; ++v) pk.insert_vertex(v);
    // A DAG built incrementally: 0->1,0->2,1->3,2->3,3->4,3->5
    std::vector<std::pair<vertex_t, vertex_t>> edges = {
        {0,1},{0,2},{1,3},{2,3},{3,4},{3,5}
    };
    DynamicGraph oracle_g;
    for (vertex_t v = 0; v < N; ++v) oracle_g.add_vertex(v);
    for (auto [u, v] : edges) {
        pk.insert_edge(u, v);
        oracle_g.add_edge(u, v);
        for (vertex_t a = 0; a < N; ++a)
            for (vertex_t b = 0; b < N; ++b) {
                bool expected = dyntest::bfs_reachable(oracle_g, a, b);
                bool got = pk.reachable(a, b);
                ASSERT(got == expected, "acyclic oracle mismatch");
            }
    }
}

// Randomized stress-oracle test for acyclic edge insertion (Task 8). This is the
// ground truth for the acyclic reorder path: it should PASS. A fixed seed makes
// every run reproducible. For each of several trials, a random topological
// permutation of N vertices is generated, then a random subset of edges that only
// go "lower -> higher" in that permutation (so the DAG stays acyclic and
// fold_cycle, which is only a Task 9 stub, is never invoked) is inserted in a
// random order into both FelinePK and a BFS-oracle DynamicGraph. After every
// single edge insertion, ALL N*N pairs are cross-checked between pk.reachable()
// and the BFS oracle.
void test_insert_edge_acyclic_stress() {
    std::mt19937 rng(987654321u); // fixed seed for determinism
    const int kTrials = 200;
    const size_t kMaxEdgesPerTrial = 60; // cap to keep the all-pairs check cheap

    std::uniform_int_distribution<int> n_dist(6, 14);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

    for (int trial = 0; trial < kTrials; ++trial) {
        vertex_t N = static_cast<vertex_t>(n_dist(rng));

        // Random topological permutation of vertex ids 0..N-1.
        std::vector<vertex_t> perm(N);
        std::iota(perm.begin(), perm.end(), 0);
        std::shuffle(perm.begin(), perm.end(), rng);

        // Candidate DAG edges (perm[i], perm[j]) with i < j are guaranteed acyclic.
        std::vector<std::pair<vertex_t, vertex_t>> edges;
        for (vertex_t i = 0; i < N && edges.size() < kMaxEdgesPerTrial; ++i) {
            for (vertex_t j = i + 1; j < N && edges.size() < kMaxEdgesPerTrial; ++j) {
                if (prob_dist(rng) < 0.25) {
                    edges.emplace_back(perm[i], perm[j]);
                }
            }
        }
        // Insert edges in a random order (not necessarily the (i,j) generation order).
        std::shuffle(edges.begin(), edges.end(), rng);

        FelinePK pk;
        DynamicGraph oracle_g;
        for (vertex_t v = 0; v < N; ++v) {
            pk.insert_vertex(v);
            oracle_g.add_vertex(v);
        }

        for (size_t e = 0; e < edges.size(); ++e) {
            vertex_t u = edges[e].first, v = edges[e].second;
            pk.insert_edge(u, v);
            oracle_g.add_edge(u, v);
            for (vertex_t a = 0; a < N; ++a) {
                for (vertex_t b = 0; b < N; ++b) {
                    bool expected = dyntest::bfs_reachable(oracle_g, a, b);
                    bool got = pk.reachable(a, b);
                    if (got != expected) {
                        std::fprintf(stderr,
                            "    stress mismatch: trial=%d N=%u edge#%zu=(%u,%u) "
                            "pair=(%u,%u) expected=%d got=%d\n",
                            trial, N, e, u, v, a, b, (int)expected, (int)got);
                    }
                    ASSERT(got == expected, "acyclic stress oracle mismatch");
                }
            }
        }
    }
}

// Fixed cycle-folding oracle: insert edges that close cycles and cross-check
// all-pairs reachability against BFS after every insertion (brief Step 1).
void test_insert_edge_cycle_oracle() {
    FelinePK pk;
    const vertex_t N = 5;
    for (vertex_t v = 0; v < N; ++v) pk.insert_vertex(v);
    DynamicGraph oracle_g;
    for (vertex_t v = 0; v < N; ++v) oracle_g.add_vertex(v);

    // Edges that create a cycle: 0->1->2->3, then 3->1 (folds {1,2,3}), then 3->4.
    std::vector<std::pair<vertex_t, vertex_t>> edges = {
        {0,1},{1,2},{2,3},{3,1},{3,4},{4,2}
    };
    for (auto [u, v] : edges) {
        pk.insert_edge(u, v);
        oracle_g.add_edge(u, v);
        for (vertex_t a = 0; a < N; ++a)
            for (vertex_t b = 0; b < N; ++b) {
                bool expected = dyntest::bfs_reachable(oracle_g, a, b);
                bool got = pk.reachable(a, b);
                ASSERT(got == expected, "cycle oracle mismatch");
            }
    }
    // After folding, 1,2,3 are mutually reachable.
    ASSERT(pk.reachable(1, 3) && pk.reachable(3, 1), "cycle: 1<->3 same SCC");
}

// Randomized CYCLIC stress oracle (Task 9 ground truth). Unlike the acyclic stress
// test, edges are drawn over ALL ordered pairs (u,v), u!=v, so cycles are allowed and
// fold_cycle is exercised heavily. Fixed seed for reproducibility. After every single
// edge insertion, ALL N*N pairs are cross-checked between pk.reachable() and BFS.
void test_insert_edge_cycle_stress() {
    std::mt19937 rng(246813579u); // fixed seed for determinism
    const int kTrials = 200;

    std::uniform_int_distribution<int> n_dist(5, 12);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

    for (int trial = 0; trial < kTrials; ++trial) {
        vertex_t N = static_cast<vertex_t>(n_dist(rng));

        // Candidate edges over ALL ordered pairs (u != v): cycles allowed.
        std::vector<std::pair<vertex_t, vertex_t>> edges;
        for (vertex_t u = 0; u < N; ++u) {
            for (vertex_t w = 0; w < N; ++w) {
                if (u == w) continue;
                if (prob_dist(rng) < 0.3) edges.emplace_back(u, w);
            }
        }
        std::shuffle(edges.begin(), edges.end(), rng);

        FelinePK pk;
        DynamicGraph oracle_g;
        for (vertex_t v = 0; v < N; ++v) {
            pk.insert_vertex(v);
            oracle_g.add_vertex(v);
        }

        for (size_t e = 0; e < edges.size(); ++e) {
            vertex_t u = edges[e].first, v = edges[e].second;
            pk.insert_edge(u, v);
            oracle_g.add_edge(u, v);
            for (vertex_t a = 0; a < N; ++a) {
                for (vertex_t b = 0; b < N; ++b) {
                    bool expected = dyntest::bfs_reachable(oracle_g, a, b);
                    bool got = pk.reachable(a, b);
                    if (got != expected) {
                        std::fprintf(stderr,
                            "    cycle stress mismatch: trial=%d N=%u edge#%zu=(%u,%u) "
                            "pair=(%u,%u) expected=%d got=%d\n",
                            trial, N, e, u, v, a, b, (int)expected, (int)got);
                    }
                    ASSERT(got == expected, "cycle stress oracle mismatch");
                }
            }
        }
    }
}

// Random edge insertions (may create cycles); verify all-pairs vs BFS at the end and
// at checkpoints. Deterministic seed for reproducibility.
void test_random_insertions_oracle() {
    const vertex_t N = 12;
    FelinePK pk;
    DynamicGraph oracle_g;
    for (vertex_t v = 0; v < N; ++v) { pk.insert_vertex(v); oracle_g.add_vertex(v); }

    std::mt19937 rng(12345);
    std::uniform_int_distribution<vertex_t> pick(0, N - 1);
    for (int step = 0; step < 40; ++step) {
        vertex_t u = pick(rng), v = pick(rng);
        if (u == v) continue;
        pk.insert_edge(u, v);
        oracle_g.add_edge(u, v);
        for (vertex_t a = 0; a < N; ++a)
            for (vertex_t b = 0; b < N; ++b) {
                bool expected = dyntest::bfs_reachable(oracle_g, a, b);
                bool got = pk.reachable(a, b);
                ASSERT(got == expected, "random oracle mismatch");
            }
    }
}

// Build FelinePK incrementally (vertex-by-vertex, then edge-by-edge in CSR order)
// from the same graph files used by the STATIC Feline tests, cross-checking
// all-pairs reachability against a BFS oracle after every single edge insertion.
// This exercises incremental construction from real static-test graphs, including
// SCC folding on cyclic.txt.
static void run_incremental_from_file(const std::string& name) {
    std::string path = "test/test_data/" + name + ".txt";
    feline::CSRGraph g = feline::load_graph(path);

    FelinePK pk;
    DynamicGraph oracle_g;
    for (feline_dyn::vertex_t v = 0; v < g.n; ++v) {
        pk.insert_vertex(v);
        oracle_g.add_vertex(v);
    }

    for (feline::vertex_t u = 0; u < g.n; ++u) {
        for (feline::edge_t e = g.out_begin[u]; e < g.out_begin[u + 1]; ++e) {
            feline::vertex_t v = g.out_adj[e];
            pk.insert_edge(u, v);
            oracle_g.add_edge(u, v);
            for (feline_dyn::vertex_t a = 0; a < g.n; ++a) {
                for (feline_dyn::vertex_t b = 0; b < g.n; ++b) {
                    bool expected = dyntest::bfs_reachable(oracle_g, a, b);
                    bool got = pk.reachable(a, b);
                    if (got != expected) {
                        std::fprintf(stderr,
                            "    incremental mismatch: graph=%s edge=(%u,%u) "
                            "pair=(%u,%u) expected=%d got=%d\n",
                            name.c_str(), u, v, a, b, (int)expected, (int)got);
                    }
                    ASSERT(got == expected,
                           ("incremental oracle mismatch: " + name).c_str());
                }
            }
        }
    }
}

void test_incremental_from_graph() {
    run_incremental_from_file("diamond");
    run_incremental_from_file("chain");
    run_incremental_from_file("tree");
    run_incremental_from_file("cyclic");
}

void test_dynamic_graph_remove_edge() {
    DynamicGraph g;
    for (vertex_t v = 0; v < 3; ++v) g.add_vertex(v);
    g.add_edge(0, 1);
    g.add_edge(0, 2);
    ASSERT(g.has_edge(0, 1) && g.has_edge(0, 2), "dg-rm: edges present");
    ASSERT(!g.has_edge(1, 0), "dg-rm: reverse edge absent");
    ASSERT(!g.has_edge(7, 8), "dg-rm: unknown vertices -> no edge");

    g.remove_edge(0, 1);
    ASSERT(!g.has_edge(0, 1), "dg-rm: edge removed");
    ASSERT(!g.succ(0).count(1), "dg-rm: out adjacency cleaned");
    ASSERT(!g.pred(1).count(0), "dg-rm: in adjacency cleaned");
    ASSERT(g.has_edge(0, 2), "dg-rm: other edge intact");

    g.remove_edge(0, 1);   // idempotent
    g.remove_edge(7, 8);   // unknown vertices: must not crash
    ASSERT(g.has_edge(0, 2), "dg-rm: still intact after no-op removals");
    ASSERT(g.out_all().size() == 3, "dg-rm: out_all exposes every vertex");
}

int main() {
    std::fprintf(stderr, "\n=== Feline-PK Dynamic Tests ===\n\n");
    RUN_TEST(test_skeleton);
    RUN_TEST(test_union_find);
    RUN_TEST(test_union_find_representative);
    RUN_TEST(test_dynamic_graph_edges);
    RUN_TEST(test_dynindex_isolated);
    RUN_TEST(test_dynindex_remove_many);
    RUN_TEST(test_build_suborder_chain);
    RUN_TEST(test_insert_remove_vertex);
    RUN_TEST(test_dyn_query_diamond);
    RUN_TEST(test_felinepk_reachable_isolated);
    RUN_TEST(test_insert_edge_acyclic_oracle);
    RUN_TEST(test_insert_edge_acyclic_stress);
    RUN_TEST(test_insert_edge_cycle_oracle);
    RUN_TEST(test_insert_edge_cycle_stress);
    RUN_TEST(test_random_insertions_oracle);
    RUN_TEST(test_incremental_from_graph);
    RUN_TEST(test_dynamic_graph_remove_edge);
    TEST_SUMMARY();
    return dyntest::tests_failed > 0 ? 1 : 0;
}
