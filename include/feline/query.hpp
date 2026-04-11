#pragma once

#include "feline/graph.hpp"
#include "feline/index.hpp"
#include <string>

namespace feline {

// Answer a single reachability query: is v reachable from u in the DAG?
// Uses FELINE Algorithm 3: positive-cut, negative-cut (dominance), level filter, iterative DFS.
bool reachable(vertex_t u, vertex_t v, const CSRGraph& dag, const FELINEIndex& idx);

// Process batch queries from a file and write results.
// Query file format: one "u v" pair per line (original vertex ids).
// Maps through comp_id before querying the DAG.
// Returns the number of queries processed.
uint64_t process_queries(const std::string& query_file,
                         const std::string& output_file,
                         const CSRGraph& dag,
                         const FELINEIndex& idx,
                         const std::vector<vertex_t>& comp_id);

// Naive BFS reachability check on the original graph (oracle).
bool naive_reachable(vertex_t u, vertex_t v, const CSRGraph& g);

// Generate a query file with BFS ground truth.
// Format: "u v result" per line, where result is 0 or 1.
// Generates num_queries random pairs from the original graph.
// Returns the number of queries generated.
uint64_t gen_queries(const std::string& output_file,
                     const CSRGraph& g,
                     uint64_t num_queries,
                     uint32_t seed = 42);

// Verify FELINE results against a ground-truth query file.
// Query file format: "u v expected" per line.
// Prints a summary of matches, mismatches, false positives, false negatives.
struct VerifyResult {
    uint64_t total = 0;
    uint64_t correct = 0;
    uint64_t wrong = 0;
    uint64_t false_positives = 0;  // FELINE says reachable, BFS says not
    uint64_t false_negatives = 0;  // FELINE says not reachable, BFS says yes
    uint64_t total_positive = 0;   // queries where expected = 1
    uint64_t total_negative = 0;   // queries where expected = 0
};

VerifyResult verify_queries(const std::string& query_file,
                            const CSRGraph& dag,
                            const FELINEIndex& idx,
                            const std::vector<vertex_t>& comp_id);

} // namespace feline
