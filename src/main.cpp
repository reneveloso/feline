#include "feline/graph.hpp"
#include "feline/index.hpp"
#include "feline/query.hpp"
#include "feline/timer.hpp"

#include <cstdio>
#include <cstring>
#include <string>

static void print_usage(const char* prog) {
    std::fprintf(stderr, "Usage:\n");
    std::fprintf(stderr, "  %s <graph_file> <query_file> [output_file]\n", prog);
    std::fprintf(stderr, "  %s --export-index <graph_file> <index.csv>\n", prog);
    std::fprintf(stderr, "  %s --gen-queries <graph_file> <output.txt> <num_queries> [seed]\n", prog);
    std::fprintf(stderr, "  %s --verify <graph_file> <queries_with_answers.txt>\n", prog);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    // Mode: --export-index
    if (std::strcmp(argv[1], "--export-index") == 0) {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }
        std::string graph_file = argv[2];
        std::string csv_file = argv[3];

        feline::Timer t;

        std::fprintf(stderr, "Loading graph: %s\n", graph_file.c_str());
        auto g = feline::load_graph(graph_file);
        std::fprintf(stderr, "  Vertices: %u, Edges: %lu (%.2f ms)\n", g.n, g.m, t.elapsed_ms());

        t.reset();
        std::fprintf(stderr, "SCC condensation...\n");
        auto cond = feline::condense(g);
        std::fprintf(stderr, "  Components: %u, DAG edges: %lu (%.2f ms)\n",
                     cond.num_components, cond.dag.m, t.elapsed_ms());

        t.reset();
        std::fprintf(stderr, "Building FELINE index...\n");
        auto idx = feline::build_index(cond.dag);
        std::fprintf(stderr, "  Index built (%.2f ms)\n", t.elapsed_ms());

        std::fprintf(stderr, "Exporting index to: %s\n", csv_file.c_str());
        feline::export_index_csv(idx, csv_file);
        std::fprintf(stderr, "Done.\n");
        return 0;
    }

    // Mode: --gen-queries
    if (std::strcmp(argv[1], "--gen-queries") == 0) {
        if (argc < 5) {
            print_usage(argv[0]);
            return 1;
        }
        std::string graph_file = argv[2];
        std::string output_file = argv[3];
        uint64_t num_queries = std::stoull(argv[4]);
        uint32_t seed = (argc >= 6) ? static_cast<uint32_t>(std::stoul(argv[5])) : 42;

        feline::Timer t;

        std::fprintf(stderr, "Loading graph: %s\n", graph_file.c_str());
        auto g = feline::load_graph(graph_file);
        std::fprintf(stderr, "  Vertices: %u, Edges: %lu (%.2f ms)\n", g.n, g.m, t.elapsed_ms());

        t.reset();
        std::fprintf(stderr, "Generating %lu queries with BFS oracle (seed=%u)...\n", num_queries, seed);
        uint64_t generated = feline::gen_queries(output_file, g, num_queries, seed);
        std::fprintf(stderr, "  Generated %lu queries (%.2f ms)\n", generated, t.elapsed_ms());
        std::fprintf(stderr, "  Output: %s\n", output_file.c_str());
        return 0;
    }

    // Mode: --verify
    if (std::strcmp(argv[1], "--verify") == 0) {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }
        std::string graph_file = argv[2];
        std::string query_file = argv[3];

        feline::Timer t;

        std::fprintf(stderr, "Loading graph: %s\n", graph_file.c_str());
        auto g = feline::load_graph(graph_file);
        std::fprintf(stderr, "  Vertices: %u, Edges: %lu (%.2f ms)\n", g.n, g.m, t.elapsed_ms());

        t.reset();
        std::fprintf(stderr, "SCC condensation...\n");
        auto cond = feline::condense(g);
        std::fprintf(stderr, "  Components: %u, DAG edges: %lu (%.2f ms)\n",
                     cond.num_components, cond.dag.m, t.elapsed_ms());

        t.reset();
        std::fprintf(stderr, "Building FELINE index...\n");
        auto idx = feline::build_index(cond.dag);
        std::fprintf(stderr, "  Index built (%.2f ms)\n", t.elapsed_ms());

        t.reset();
        std::fprintf(stderr, "Verifying against: %s\n", query_file.c_str());
        auto res = feline::verify_queries(query_file, cond.dag, idx, cond.comp_id);
        double verify_ms = t.elapsed_ms();

        std::fprintf(stderr, "\n");
        std::printf("=== Verification Results ===\n");
        std::printf("Total queries:      %lu\n", res.total);
        std::printf("  Positive (BFS=1): %lu\n", res.total_positive);
        std::printf("  Negative (BFS=0): %lu\n", res.total_negative);
        std::printf("\n");
        std::printf("Correct:            %lu (%.2f%%)\n", res.correct,
                    res.total > 0 ? 100.0 * res.correct / res.total : 0.0);
        std::printf("Wrong:              %lu (%.2f%%)\n", res.wrong,
                    res.total > 0 ? 100.0 * res.wrong / res.total : 0.0);
        if (res.wrong > 0) {
            std::printf("  False positives:  %lu (FELINE=1, BFS=0)\n", res.false_positives);
            std::printf("  False negatives:  %lu (FELINE=0, BFS=1)\n", res.false_negatives);
        }
        std::printf("\n");
        std::printf("Verification time:  %.2f ms (%.4f ms/query)\n",
                    verify_ms, res.total > 0 ? verify_ms / res.total : 0.0);

        return res.wrong > 0 ? 1 : 0;
    }

    // Normal mode: graph_file query_file [output_file]
    std::string graph_file = argv[1];
    std::string query_file = argv[2];
    std::string output_file = (argc >= 4) ? argv[3] : "";

    feline::Timer total;

    // Load graph
    feline::Timer t;
    std::fprintf(stderr, "Loading graph: %s\n", graph_file.c_str());
    auto g = feline::load_graph(graph_file);
    std::fprintf(stderr, "  Vertices: %u, Edges: %lu (%.2f ms)\n", g.n, g.m, t.elapsed_ms());

    // SCC condensation
    t.reset();
    std::fprintf(stderr, "SCC condensation...\n");
    auto cond = feline::condense(g);
    std::fprintf(stderr, "  Components: %u, DAG edges: %lu (%.2f ms)\n",
                 cond.num_components, cond.dag.m, t.elapsed_ms());

    // Build index
    t.reset();
    std::fprintf(stderr, "Building FELINE index...\n");
    auto idx = feline::build_index(cond.dag);
    std::fprintf(stderr, "  Index built (%.2f ms)\n", t.elapsed_ms());

    // Process queries
    t.reset();
    std::fprintf(stderr, "Processing queries: %s\n", query_file.c_str());
    uint64_t num_queries = feline::process_queries(query_file, output_file, cond.dag, idx, cond.comp_id);
    double query_ms = t.elapsed_ms();
    std::fprintf(stderr, "  Queries: %lu, Total: %.2f ms, Avg: %.4f ms/query\n",
                 num_queries, query_ms, num_queries > 0 ? query_ms / num_queries : 0.0);

    std::fprintf(stderr, "Total time: %.2f ms\n", total.elapsed_ms());
    return 0;
}
