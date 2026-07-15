// Converts a graph from adjacency list format to edge list format.
//
// Input format (adjacency list):
//   <num_vertices>
//   0: 3 5 7 #
//   1: #
//   2: 0 4 #
//   ...
//
// Output format (edge list, used by feline):
//   <num_vertices> <num_edges>
//   0 3
//   0 5
//   0 7
//   2 0
//   2 4
//   ...
//
// Usage: convert_adjlist <input.gra> <output.txt>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <input.gra> <output.txt>\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    const char* output_file = argv[2];

    FILE* fin = std::fopen(input_file, "r");
    if (!fin) {
        std::fprintf(stderr, "Error: cannot open input file: %s\n", input_file);
        return 1;
    }

    // Read number of vertices
    uint32_t n;
    if (std::fscanf(fin, "%u", &n) != 1) {
        std::fprintf(stderr, "Error: cannot read number of vertices\n");
        std::fclose(fin);
        return 1;
    }
    std::fprintf(stderr, "Vertices: %u\n", n);

    // First pass: read all edges and count them
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    edges.reserve(n); // initial estimate

    char line[1024 * 1024]; // 1MB line buffer for vertices with many successors
    // consume rest of first line
    if (std::fgets(line, sizeof(line), fin) == nullptr) {}

    uint32_t lines_read = 0;
    while (std::fgets(line, sizeof(line), fin)) {
        // Parse "vertex: succ1 succ2 ... #"
        char* p = line;

        // Read vertex id
        uint32_t u = 0;
        while (*p >= '0' && *p <= '9') {
            u = u * 10 + (*p - '0');
            p++;
        }

        // Skip ": "
        while (*p == ':' || *p == ' ' || *p == '\t') p++;

        // Read successors until '#' or end of line
        while (*p && *p != '#' && *p != '\n' && *p != '\r') {
            // Skip whitespace
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '#' || *p == '\n' || *p == '\r' || !*p) break;

            // Read successor
            uint32_t v = 0;
            while (*p >= '0' && *p <= '9') {
                v = v * 10 + (*p - '0');
                p++;
            }
            edges.push_back({u, v});
        }

        lines_read++;
        if (lines_read % 1000000 == 0) {
            std::fprintf(stderr, "  Read %u / %u vertices...\r", lines_read, n);
        }
    }
    std::fclose(fin);

    uint64_t m = edges.size();
    std::fprintf(stderr, "  Read %u vertices, %lu edges\n", lines_read, m);

    // Write output
    FILE* fout = std::fopen(output_file, "w");
    if (!fout) {
        std::fprintf(stderr, "Error: cannot open output file: %s\n", output_file);
        return 1;
    }

    std::fprintf(fout, "%u %lu\n", n, m);
    for (auto& [u, v] : edges) {
        std::fprintf(fout, "%u %u\n", u, v);
    }
    std::fclose(fout);

    std::fprintf(stderr, "Output: %s (%u vertices, %lu edges)\n", output_file, n, m);
    return 0;
}
