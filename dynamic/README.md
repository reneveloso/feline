# Dynamic Reachability Index

Incremental (dynamic) extension of Feline. Maintains the Feline index (two topological
orderings / 2D coordinates) as the graph changes, instead of rebuilding it on every
update.

**Status:** vertex insert/remove, edge insertion (with SCC folding), edge removal (with
SCC splitting), and dynamic queries. Batch insertion is future work.

## Building

Dynamic code is off by default. Enable it from the repository root:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFELINE_BUILD_DYNAMIC=ON
    cmake --build build -j"$(nproc)"

## Testing

    ./build/dynamic/feline_dyn_tests

## Demo

    ./build/dynamic/dyn_demo <updates_file>

## Demo input format

One command per line:

    v <id>        insert a disconnected vertex
    e <u> <v>     insert edge (u, v)
    q <u> <v>     query reachability; prints "<u> <v> <0|1>"

## Known limitations / future work

- Not yet implemented: batch insertion.
- Removing an edge from inside a component enumerates that component by scanning the
  vertex set, which is O(|V|) for that operation. Keeping a member list per component
  would make it proportional to the component instead; not done until profiling shows
  it matters.
