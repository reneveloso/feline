# FELINE - Fast rEfined onLINE search

> 🇧🇷 Uma versão deste documento em português está disponível em [README.pt-BR.md](README.pt-BR.md).

C++ implementation of the FELINE algorithm for reachability queries on directed graphs, as described in the paper:

> **Reachability Queries in Very Large Graphs: A Fast Refined Online Search Approach**
> Veloso, R. R.; Cerf, L.; Meira Jr, W.; Zaki, M. J.
> EDBT 2014. DOI: 10.5441/002/edbt.2014.46

Given a directed graph G and two vertices u and v, FELINE efficiently answers whether a path from u to v exists.

## How it works

FELINE builds an index that assigns each vertex a pair of coordinates (X, Y) in a 2D plane (Dominance Drawing). The fundamental property is:

- If u reaches v, then `X[u] <= X[v]` **and** `Y[u] <= Y[v]`
- If this relation does **not** hold, u does **not** reach v (decided in constant time)

When the relation holds but is inconclusive, a pruned DFS is performed over a reduced subgraph.

### Filters applied during queries

1. **Positive-cut**: min-post intervals from a spanning tree decide positive reachability in O(1)
2. **Negative-cut**: the dominance relation (X, Y) rules out non-reachability in O(1)
3. **Level filter**: vertices at the same or a lower level cannot be reached

## Internal architecture

### Data structures

**CSR graph (Compressed Sparse Row)** -- compact, cache-friendly representation:

```cpp
using vertex_t = uint32_t;
using edge_t   = uint64_t;

struct CSRGraph {
    uint32_t n;                      // vertices
    uint64_t m;                      // edges
    std::vector<edge_t> out_begin;   // size n+1, start of successors
    std::vector<vertex_t> out_adj;   // size m, contiguous successors
};
```

**FELINE index** -- 5 arrays of size |V| (linear space):

```cpp
struct FELINEIndex {
    std::vector<uint32_t> x_rank;         // X coordinate (topological order)
    std::vector<uint32_t> y_rank;         // Y coordinate (Kornaropoulos heuristic)
    std::vector<uint32_t> interval_start; // s_u (positive-cut, min-post)
    std::vector<uint32_t> interval_end;   // e_u (positive-cut, min-post)
    std::vector<uint32_t> level;          // level filter
};
```

### Index construction pipeline

1. **SCC condensation** (`graph.cpp`): **iterative** Tarjan with an explicit stack. Collapses strongly connected components into single vertices, producing a DAG. Complexity: O(|V| + |E|).

2. **X coordinate** (`index.cpp`): Kahn's algorithm (topological sort via BFS). `x_rank[u]` = position of u in the ordering. Complexity: O(|V| + |E|).

3. **Y coordinate** (`index.cpp`): Kornaropoulos heuristic -- a second topological sort where, at each step, the root with the largest x_rank is selected (max-heap). Locally minimizes the number of false positives. Complexity: O(|V| log |V| + |E|).

4. **Min-post intervals** (`index.cpp`): **iterative** DFS extracts a spanning tree/forest and computes post-order. `interval_end[u] = post_order[u]`. `interval_start[u] = min(interval_start[children])`, or `interval_end[u]` if a leaf. Check: `I_v ⊆ I_u` <=> `start[u] <= start[v] && end[v] <= end[u]`. Complexity: O(|V| + |E|).

5. **Levels** (`index.cpp`): processes vertices in topological order. `level[v] = 0` if a root, otherwise `level[v] = max(level[u] + 1)` over predecessors u. Complexity: O(|V| + |E|).

### Query algorithm (Algorithm 3 from the paper)

For `reachable(u, v)`, an **iterative** DFS with three pruning filters:

1. **Positive-cut**: if `I_v ⊆ I_u` -> return true (O(1))
2. **Reflexivity**: if `u == v` -> return true
3. **Negative-cut + level filter**: if `x[u] <= x[v] AND y[u] <= y[v] AND level[u] < level[v]`, explore successors; otherwise, prune the branch

All algorithms use explicit stacks (never recursion) to support graphs with millions of vertices without stack overflow.

## Requirements

- C++17 (g++ >= 7 or clang++ >= 5)
- CMake >= 3.14
- Python 3 with `matplotlib` and `numpy` (visualization only)

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

For a debug build with sanitizers:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

## Tests

Run from the project root (tests reference relative paths):

```bash
cd /path/to/feline
./build/feline_tests
```

The 16 unit tests cover:

| Group | Tests | What it verifies |
|-------|-------|------------------|
| Graph loading | 2 | correct n, m; CSR adjacency |
| SCC condensation | 2 | pure DAG: identity; cyclic: correct SCCs, condensed DAG |
| X coordinates | 1 | for every edge (u,v): x_rank[u] < x_rank[v]; unique ranks in [0,n) |
| Y coordinates | 1 | for every edge (u,v): y_rank[u] < y_rank[v]; unique ranks |
| Min-post intervals | 1 | root contains all intervals; leaves: start == end |
| Levels | 2 | roots at level 0; level grows along edges |
| Queries | 4 | specific cases on each test graph |
| Exhaustive (BFS oracle) | 2 | all pairs verified against naive BFS |
| CSV export | 1 | generated CSV has all vertices with correct values |

Test graphs in `test/test_data/`: diamond (4v), chain (5v), tree (7v), cyclic (6v with 2 SCCs).

## Usage

### Process reachability queries

```bash
./build/feline <graph> <queries> [output]
```

Example:

```bash
./build/feline test/test_data/diamond.txt queries.txt results.txt
```

If the output file is omitted, results are not written (useful for timing only).

The stderr output shows per-phase timings:

```
Loading graph: test/test_data/diamond.txt
  Vertices: 4, Edges: 4 (0.27 ms)
SCC condensation...
  Components: 4, DAG edges: 4 (0.15 ms)
Building FELINE index...
  Index built (0.17 ms)
Processing queries: queries.txt
  Queries: 5, Total: 0.11 ms, Avg: 0.0224 ms/query
Total time: 0.70 ms
```

### Export the index for visualization

```bash
./build/feline --export-index <graph> <index.csv>
```

Example:

```bash
./build/feline --export-index test/test_data/diamond.txt index.csv
```

### Generate queries with a BFS ground truth

Generates random vertex pairs and runs an exhaustive BFS to determine the actual reachability of each pair. The result is a file with the ground truth.

```bash
./build/feline --gen-queries <graph> <output.txt> <num_queries> [seed]
```

Example:

```bash
./build/feline --gen-queries test/test_data/diamond.txt queries_bfs.txt 1000
```

The generated file has the format `source destination result`:

```
1 3 1
3 0 0
2 3 1
0 0 1
```

The `seed` parameter (default: 42) controls the random generation for reproducibility.

### Verify FELINE against the BFS ground truth

Given a query file with a ground truth (produced by the command above), runs FELINE on each query and compares the result with the expected one.

```bash
./build/feline --verify <graph> <queries_with_ground_truth.txt>
```

Example:

```bash
./build/feline --verify test/test_data/diamond.txt queries_bfs.txt
```

Output:

```
=== Verification Results ===
Total queries:      20
  Positive (BFS=1): 12
  Negative (BFS=0): 8

Correct:            20 (100.00%)
Wrong:              0 (0.00%)

Verification time:  0.09 ms (0.0046 ms/query)
```

On errors, the report details false positives (FELINE=1, BFS=0) and false negatives (FELINE=0, BFS=1). The command exits with code 1 if any errors are found.

### Visualize the 2D index

```bash
python3 tools/plot_index.py index.csv -o chart.png --edges graph.txt
```

Options:
- `-o file.png` -- save to a file (PNG/SVG/PDF). If omitted, opens an interactive window.
- `--edges graph.txt` -- draw the graph edges as arrows.
- `--no-labels` -- hide vertex labels (recommended for large graphs).

### Convert adjacency-list graphs

Some datasets are distributed in adjacency-list format. The `convert_adjlist` tool converts them to the edge-list format used by `feline`:

```bash
./build/convert_adjlist <input.gra> <output.txt>
```

> **Datasets.** The real-world benchmark graphs used in the paper are available
> from the GRAIL project archive, in `.gra` (adjacency-list) format:
> <https://code.google.com/archive/p/grail/downloads>. Download the `.gra`
> file and convert it with `convert_adjlist` before running `feline`.
> The largest dataset (`citeseerx`, 217 MB) is not tracked in this repository
> due to GitHub's 100 MB per-file limit — obtain it from the archive above.

## File formats

### Input graph

Text file with an edge list. Vertices are 0-indexed integers.

```
<num_vertices> <num_edges>
<u0> <v0>
<u1> <v1>
...
```

Example (diamond graph):

```
4 4
0 1
0 2
1 3
2 3
```

The graph may contain cycles -- SCC condensation is applied automatically.

### Query file (default mode)

One vertex pair per line:

```
<source> <destination>
<source> <destination>
...
```

Example:

```
0 3
3 0
1 2
```

### Query file with ground truth (--gen-queries / --verify modes)

Three fields per line: source, destination, and the BFS result (0 or 1):

```
<source> <destination> <reachable>
<source> <destination> <reachable>
...
```

Example:

```
1 3 1
3 0 0
2 3 1
0 0 1
```

### Output file

One line per query, with `1` (reachable) or `0` (not reachable):

```
1
0
0
```

### Exported index CSV

A header followed by one line per vertex of the condensed DAG:

```
vertex,x,y,level
0,0,0,0
1,1,2,1
2,2,1,1
3,3,3,2
```

Fields:
- `vertex` -- vertex identifier in the condensed DAG
- `x` -- X coordinate (position in the first topological sort)
- `y` -- Y coordinate (position in the Kornaropoulos ordering)
- `level` -- vertex level (longest distance from a root)

## Project structure

```
feline/
├── CMakeLists.txt          # Build system
├── README.md               # This file (English)
├── README.pt-BR.md         # Portuguese version
├── include/feline/
│   ├── graph.hpp           # CSR graph, SCC condensation
│   ├── index.hpp           # FELINE index
│   ├── query.hpp           # Query engine, BFS oracle, verification
│   └── timer.hpp           # Time measurement
├── src/
│   ├── graph.cpp           # Graph loading + iterative Tarjan
│   ├── index.cpp           # X, Y, min-post intervals, levels
│   ├── query.cpp           # Algorithm 3 (iterative pruned DFS), BFS oracle, generation and verification
│   └── main.cpp            # CLI (query, export-index, gen-queries, verify)
├── tools/
│   ├── convert_adjlist.cpp # Adjacency-list to edge-list converter
│   └── plot_index.py       # 2D visualization with matplotlib
├── test/
│   ├── test_main.cpp       # 16 unit tests
│   └── test_data/          # Test graphs
│       ├── diamond.txt     # diamond DAG (4 vertices)
│       ├── chain.txt       # linear chain (5 vertices)
│       ├── tree.txt        # binary tree (7 vertices)
│       └── cyclic.txt      # graph with cycles (6 vertices, 2 SCCs)
└── dynamic/                # Dynamic extension -- optional subproject
    ├── README.md           # Dynamic index docs
    ├── include/feline_dyn/ # Mutable graph, union-find, orderings, query, facade
    ├── src/                # Implementation
    ├── test/               # Oracle-based unit + stress tests
    └── tools/              # dyn_demo, dyn_incremental
```

## Complexity

| Operation | Time | Space |
|-----------|------|-------|
| SCC condensation | O(\|V\| + \|E\|) | O(\|V\| + \|E\|) |
| Index construction | O(\|V\| log \|V\| + \|E\|) | O(\|V\|) |
| Query (negative-cut) | O(1) | - |
| Query (positive-cut) | O(1) | - |
| Query (worst case) | O(\|V\| + \|E\|) | O(\|V\|) |

## Dynamic index

The static index above is built once for a fixed graph. The **`dynamic/`** subproject
maintains the index **incrementally** as the graph changes, instead of rebuilding it on
every update: disconnected vertex insert/remove, edge insertion with incremental reorder
of the affected region and strongly-connected-component folding when an edge closes a
cycle, and a coordinate-based dynamic query. Edge removal and batch insertion are future
work.

The subproject is **off by default**; the static build is unchanged unless you enable it:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFELINE_BUILD_DYNAMIC=ON
cmake --build build -j"$(nproc)"
./build/dynamic/feline_dyn_tests          # unit + all-pairs BFS-oracle stress tests
./build/dynamic/dyn_incremental test/test_data/cyclic.txt   # observe incremental build
```

See [`dynamic/README.md`](dynamic/README.md) for details. Correctness is verified against
an all-pairs BFS oracle after every insertion, including randomized stress tests (up to
thousands of SCC folds on dense random digraphs).

## Reference

```bibtex
@inproceedings{veloso2014feline,
  title={Reachability Queries in Very Large Graphs: A Fast Refined Online Search Approach},
  author={Veloso, Ren{\^e} R. and Cerf, Lo{\"i}c and Meira Jr, Wagner and Zaki, Mohammed J.},
  booktitle={Proc. 17th International Conference on Extending Database Technology (EDBT)},
  pages={511--522},
  year={2014},
  doi={10.5441/002/edbt.2014.46}
}
```
