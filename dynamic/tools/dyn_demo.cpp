// Applies a sequence of updates to a Feline-PK index and answers queries.
//
// Input file format (one command per line):
//   v <id>          insert disconnected vertex
//   e <u> <v>       insert edge (u, v)
//   q <u> <v>       query reachability; prints "<u> <v> <0|1>"
//
// Usage: dyn_demo <updates_file>
#include "feline_dyn/feline_pk.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <updates_file>\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "Cannot open %s\n", argv[1]);
        return 2;
    }
    feline_dyn::FelinePK pk;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        char cmd;
        if (!(ss >> cmd)) continue;
        if (cmd == 'v') {
            uint32_t id; ss >> id; pk.insert_vertex(id);
        } else if (cmd == 'e') {
            uint32_t u, v; ss >> u >> v; pk.insert_edge(u, v);
        } else if (cmd == 'q') {
            uint32_t u, v; ss >> u >> v;
            std::printf("%u %u %d\n", u, v, pk.reachable(u, v) ? 1 : 0);
        }
    }
    return 0;
}
