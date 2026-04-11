#include "feline/graph.hpp"
#include "feline/index.hpp"
#include "feline/query.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <set>
#include <string>
#include <vector>

// ---------- Test framework ----------

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg)                                                 \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::fprintf(stderr, "  FAIL: %s (line %d): %s\n",           \
                         msg, __LINE__, #cond);                           \
            tests_failed++;                                               \
            return;                                                       \
        }                                                                 \
    } while (0)

#define RUN_TEST(fn)                                                      \
    do {                                                                  \
        tests_run++;                                                      \
        std::fprintf(stderr, "  Running: %s ... ", #fn);                  \
        fn();                                                             \
        if (tests_failed == prev_failed) {                                \
            tests_passed++;                                               \
            std::fprintf(stderr, "OK\n");                                 \
        }                                                                 \
    } while (0)

// ---------- Helpers ----------

static std::string test_data(const std::string& name) {
    return "test/test_data/" + name;
}

// Use the naive_reachable from query.hpp
using feline::naive_reachable;

// Get successors of a vertex as a sorted set
static std::set<feline::vertex_t> successors(const feline::CSRGraph& g, feline::vertex_t u) {
    std::set<feline::vertex_t> s;
    for (feline::edge_t e = g.out_begin[u]; e < g.out_begin[u + 1]; ++e) {
        s.insert(g.out_adj[e]);
    }
    return s;
}

// ---------- Test: Graph loading ----------

void test_graph_diamond() {
    auto g = feline::load_graph(test_data("diamond.txt"));
    ASSERT(g.n == 4, "diamond: n == 4");
    ASSERT(g.m == 4, "diamond: m == 4");
    auto s0 = successors(g, 0);
    ASSERT(s0.count(1) && s0.count(2), "diamond: 0 -> {1,2}");
    auto s1 = successors(g, 1);
    ASSERT(s1.count(3) && s1.size() == 1, "diamond: 1 -> {3}");
    auto s3 = successors(g, 3);
    ASSERT(s3.empty(), "diamond: 3 -> {}");
}

void test_graph_chain() {
    auto g = feline::load_graph(test_data("chain.txt"));
    ASSERT(g.n == 5, "chain: n == 5");
    ASSERT(g.m == 4, "chain: m == 4");
    for (uint32_t i = 0; i < 4; ++i) {
        auto s = successors(g, i);
        ASSERT(s.size() == 1 && s.count(i + 1), "chain: edge");
    }
}

// ---------- Test: SCC condensation ----------

void test_scc_dag() {
    auto g = feline::load_graph(test_data("diamond.txt"));
    auto cond = feline::condense(g);
    ASSERT(cond.num_components == 4, "diamond SCC: 4 components");
    // Each vertex is its own component
    std::set<feline::vertex_t> ids(cond.comp_id.begin(), cond.comp_id.end());
    ASSERT(ids.size() == 4, "diamond SCC: 4 distinct comp_ids");
}

void test_scc_cyclic() {
    auto g = feline::load_graph(test_data("cyclic.txt"));
    auto cond = feline::condense(g);
    ASSERT(cond.num_components == 2, "cyclic SCC: 2 components");
    // Vertices 0,1,2 should be in same component
    ASSERT(cond.comp_id[0] == cond.comp_id[1], "cyclic: 0,1 same SCC");
    ASSERT(cond.comp_id[1] == cond.comp_id[2], "cyclic: 1,2 same SCC");
    // Vertices 3,4,5 should be in same component
    ASSERT(cond.comp_id[3] == cond.comp_id[4], "cyclic: 3,4 same SCC");
    ASSERT(cond.comp_id[4] == cond.comp_id[5], "cyclic: 4,5 same SCC");
    // The two components should be different
    ASSERT(cond.comp_id[0] != cond.comp_id[3], "cyclic: different SCCs");
    // DAG should have 1 edge
    ASSERT(cond.dag.n == 2, "cyclic DAG: 2 vertices");
    ASSERT(cond.dag.m == 1, "cyclic DAG: 1 edge");
}

// ---------- Test: X rank (topological order) ----------

void test_x_rank() {
    auto g = feline::load_graph(test_data("diamond.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    // For every edge (u,v) in DAG: x_rank[u] < x_rank[v]
    for (uint32_t u = 0; u < cond.dag.n; ++u) {
        for (feline::edge_t e = cond.dag.out_begin[u]; e < cond.dag.out_begin[u + 1]; ++e) {
            feline::vertex_t v = cond.dag.out_adj[e];
            ASSERT(idx.x_rank[u] < idx.x_rank[v], "x_rank: topo order");
        }
    }

    // All ranks are unique and in [0, n)
    std::set<uint32_t> ranks(idx.x_rank.begin(), idx.x_rank.end());
    ASSERT(ranks.size() == cond.dag.n, "x_rank: all unique");
    ASSERT(*ranks.begin() == 0, "x_rank: starts at 0");
    ASSERT(*ranks.rbegin() == cond.dag.n - 1, "x_rank: ends at n-1");
}

// ---------- Test: Y rank ----------

void test_y_rank() {
    auto g = feline::load_graph(test_data("diamond.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    // For every edge (u,v) in DAG: y_rank[u] < y_rank[v]
    for (uint32_t u = 0; u < cond.dag.n; ++u) {
        for (feline::edge_t e = cond.dag.out_begin[u]; e < cond.dag.out_begin[u + 1]; ++e) {
            feline::vertex_t v = cond.dag.out_adj[e];
            ASSERT(idx.y_rank[u] < idx.y_rank[v], "y_rank: topo order");
        }
    }

    // All ranks unique
    std::set<uint32_t> ranks(idx.y_rank.begin(), idx.y_rank.end());
    ASSERT(ranks.size() == cond.dag.n, "y_rank: all unique");
}

// ---------- Test: Intervals (positive-cut) ----------

void test_intervals_chain() {
    auto g = feline::load_graph(test_data("chain.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    // Leaf (last vertex in chain): start == end
    // Find the leaf: vertex with no successors
    for (uint32_t u = 0; u < cond.dag.n; ++u) {
        if (cond.dag.out_begin[u] == cond.dag.out_begin[u + 1]) {
            ASSERT(idx.interval_start[u] == idx.interval_end[u], "intervals: leaf start==end");
        }
    }

    // Root should contain all vertices' intervals
    // Find root: build in-degree, look for in_deg == 0
    std::vector<uint32_t> in_deg(cond.dag.n, 0);
    for (uint32_t u = 0; u < cond.dag.n; ++u) {
        for (feline::edge_t e = cond.dag.out_begin[u]; e < cond.dag.out_begin[u + 1]; ++e) {
            in_deg[cond.dag.out_adj[e]]++;
        }
    }
    for (uint32_t u = 0; u < cond.dag.n; ++u) {
        if (in_deg[u] == 0) {
            // Root interval should contain all others (in a chain)
            for (uint32_t v = 0; v < cond.dag.n; ++v) {
                ASSERT(idx.interval_start[u] <= idx.interval_start[v] &&
                       idx.interval_end[v] <= idx.interval_end[u],
                       "intervals: root contains all (chain)");
            }
        }
    }
}

// ---------- Test: Levels ----------

void test_levels_chain() {
    auto g = feline::load_graph(test_data("chain.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    // In a chain 0->1->2->3->4, after condensation (identity mapping),
    // level should increase along edges
    for (uint32_t u = 0; u < cond.dag.n; ++u) {
        for (feline::edge_t e = cond.dag.out_begin[u]; e < cond.dag.out_begin[u + 1]; ++e) {
            feline::vertex_t v = cond.dag.out_adj[e];
            ASSERT(idx.level[u] < idx.level[v], "levels: increases along edges");
        }
    }

    // Roots have level 0
    std::vector<uint32_t> in_deg(cond.dag.n, 0);
    for (uint32_t u = 0; u < cond.dag.n; ++u) {
        for (feline::edge_t e = cond.dag.out_begin[u]; e < cond.dag.out_begin[u + 1]; ++e) {
            in_deg[cond.dag.out_adj[e]]++;
        }
    }
    for (uint32_t u = 0; u < cond.dag.n; ++u) {
        if (in_deg[u] == 0) {
            ASSERT(idx.level[u] == 0, "levels: root == 0");
        }
    }
}

void test_levels_tree() {
    auto g = feline::load_graph(test_data("tree.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    // Root (0 in original graph) should have level 0
    feline::vertex_t root_comp = cond.comp_id[0];
    ASSERT(idx.level[root_comp] == 0, "tree levels: root == 0");

    // Leaves (3,4,5,6) should have level 2
    for (uint32_t v : {3u, 4u, 5u, 6u}) {
        feline::vertex_t cv = cond.comp_id[v];
        ASSERT(idx.level[cv] == 2, "tree levels: leaf == 2");
    }
}

// ---------- Test: Reachability queries ----------

void test_reachable_diamond() {
    auto g = feline::load_graph(test_data("diamond.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    auto reach = [&](uint32_t u, uint32_t v) -> bool {
        feline::vertex_t cu = cond.comp_id[u], cv = cond.comp_id[v];
        if (cu == cv) return true;
        return feline::reachable(cu, cv, cond.dag, idx);
    };

    ASSERT(reach(0, 3) == true,  "diamond: r(0,3)=true");
    ASSERT(reach(3, 0) == false, "diamond: r(3,0)=false");
    ASSERT(reach(1, 2) == false, "diamond: r(1,2)=false");
    ASSERT(reach(0, 0) == true,  "diamond: r(0,0)=true (reflexive)");
    ASSERT(reach(0, 1) == true,  "diamond: r(0,1)=true");
    ASSERT(reach(0, 2) == true,  "diamond: r(0,2)=true");
}

void test_reachable_chain() {
    auto g = feline::load_graph(test_data("chain.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    auto reach = [&](uint32_t u, uint32_t v) -> bool {
        feline::vertex_t cu = cond.comp_id[u], cv = cond.comp_id[v];
        if (cu == cv) return true;
        return feline::reachable(cu, cv, cond.dag, idx);
    };

    ASSERT(reach(0, 4) == true,  "chain: r(0,4)=true");
    ASSERT(reach(4, 0) == false, "chain: r(4,0)=false");
    ASSERT(reach(2, 4) == true,  "chain: r(2,4)=true");
    ASSERT(reach(2, 0) == false, "chain: r(2,0)=false");
}

void test_reachable_tree() {
    auto g = feline::load_graph(test_data("tree.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    auto reach = [&](uint32_t u, uint32_t v) -> bool {
        feline::vertex_t cu = cond.comp_id[u], cv = cond.comp_id[v];
        if (cu == cv) return true;
        return feline::reachable(cu, cv, cond.dag, idx);
    };

    ASSERT(reach(0, 6) == true,  "tree: r(0,6)=true");
    ASSERT(reach(1, 5) == false, "tree: r(1,5)=false");
    ASSERT(reach(3, 4) == false, "tree: r(3,4)=false");
    ASSERT(reach(0, 3) == true,  "tree: r(0,3)=true");
}

void test_reachable_cyclic() {
    auto g = feline::load_graph(test_data("cyclic.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    auto reach = [&](uint32_t u, uint32_t v) -> bool {
        feline::vertex_t cu = cond.comp_id[u], cv = cond.comp_id[v];
        if (cu == cv) return true;
        return feline::reachable(cu, cv, cond.dag, idx);
    };

    // 0,1,2 are in same SCC: reachable from each other
    ASSERT(reach(0, 1) == true, "cyclic: r(0,1)=true (same SCC)");
    ASSERT(reach(1, 0) == true, "cyclic: r(1,0)=true (same SCC)");
    // 0 reaches 5 (via SCC edge)
    ASSERT(reach(0, 5) == true, "cyclic: r(0,5)=true");
    // 3 does not reach 0
    ASSERT(reach(3, 0) == false, "cyclic: r(3,0)=false");
}

// Exhaustive test: verify FELINE against BFS oracle for all pairs
void test_reachable_exhaustive_diamond() {
    auto g = feline::load_graph(test_data("diamond.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    for (uint32_t u = 0; u < g.n; ++u) {
        for (uint32_t v = 0; v < g.n; ++v) {
            bool expected = naive_reachable(u, v, g);

            feline::vertex_t cu = cond.comp_id[u], cv = cond.comp_id[v];
            bool actual;
            if (cu == cv) {
                actual = true;
            } else {
                actual = feline::reachable(cu, cv, cond.dag, idx);
            }

            ASSERT(actual == expected, "exhaustive diamond");
        }
    }
}

void test_reachable_exhaustive_tree() {
    auto g = feline::load_graph(test_data("tree.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    for (uint32_t u = 0; u < g.n; ++u) {
        for (uint32_t v = 0; v < g.n; ++v) {
            bool expected = naive_reachable(u, v, g);

            feline::vertex_t cu = cond.comp_id[u], cv = cond.comp_id[v];
            bool actual;
            if (cu == cv) {
                actual = true;
            } else {
                actual = feline::reachable(cu, cv, cond.dag, idx);
            }

            ASSERT(actual == expected, "exhaustive tree");
        }
    }
}

// ---------- Test: Export ----------

void test_export_index() {
    auto g = feline::load_graph(test_data("diamond.txt"));
    auto cond = feline::condense(g);
    auto idx = feline::build_index(cond.dag);

    std::string tmpfile = "/tmp/feline_test_export.csv";
    feline::export_index_csv(idx, tmpfile);

    // Read it back and verify
    FILE* f = std::fopen(tmpfile.c_str(), "r");
    ASSERT(f != nullptr, "export: file created");

    char header[256];
    ASSERT(std::fgets(header, sizeof(header), f) != nullptr, "export: header");

    uint32_t count = 0;
    uint32_t vertex, x, y, level;
    while (std::fscanf(f, "%u,%u,%u,%u", &vertex, &x, &y, &level) == 4) {
        ASSERT(vertex < cond.dag.n, "export: valid vertex");
        ASSERT(x == idx.x_rank[vertex], "export: x matches");
        ASSERT(y == idx.y_rank[vertex], "export: y matches");
        ASSERT(level == idx.level[vertex], "export: level matches");
        count++;
    }
    std::fclose(f);
    ASSERT(count == cond.dag.n, "export: all vertices present");
    std::remove(tmpfile.c_str());
}

// ---------- Main ----------

int main() {
    int prev_failed = 0;

    std::fprintf(stderr, "\n=== FELINE Unit Tests ===\n\n");

    std::fprintf(stderr, "[Graph Loading]\n");
    prev_failed = tests_failed; RUN_TEST(test_graph_diamond);
    prev_failed = tests_failed; RUN_TEST(test_graph_chain);

    std::fprintf(stderr, "\n[SCC Condensation]\n");
    prev_failed = tests_failed; RUN_TEST(test_scc_dag);
    prev_failed = tests_failed; RUN_TEST(test_scc_cyclic);

    std::fprintf(stderr, "\n[Index - X Rank]\n");
    prev_failed = tests_failed; RUN_TEST(test_x_rank);

    std::fprintf(stderr, "\n[Index - Y Rank]\n");
    prev_failed = tests_failed; RUN_TEST(test_y_rank);

    std::fprintf(stderr, "\n[Index - Intervals]\n");
    prev_failed = tests_failed; RUN_TEST(test_intervals_chain);

    std::fprintf(stderr, "\n[Index - Levels]\n");
    prev_failed = tests_failed; RUN_TEST(test_levels_chain);
    prev_failed = tests_failed; RUN_TEST(test_levels_tree);

    std::fprintf(stderr, "\n[Reachability Queries]\n");
    prev_failed = tests_failed; RUN_TEST(test_reachable_diamond);
    prev_failed = tests_failed; RUN_TEST(test_reachable_chain);
    prev_failed = tests_failed; RUN_TEST(test_reachable_tree);
    prev_failed = tests_failed; RUN_TEST(test_reachable_cyclic);

    std::fprintf(stderr, "\n[Exhaustive Reachability (BFS Oracle)]\n");
    prev_failed = tests_failed; RUN_TEST(test_reachable_exhaustive_diamond);
    prev_failed = tests_failed; RUN_TEST(test_reachable_exhaustive_tree);

    std::fprintf(stderr, "\n[Export]\n");
    prev_failed = tests_failed; RUN_TEST(test_export_index);

    std::fprintf(stderr, "\n=== Results: %d/%d passed, %d failed ===\n\n",
                 tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
