# Dynamic Reachability Index

Incremental (dynamic) extension of Feline. Maintains the Feline index (two topological
orderings / 2D coordinates) as the graph changes, instead of rebuilding it on every
update.

**Status (first increment):** vertex insert/remove, edge insertion (with SCC folding),
and dynamic queries. Edge removal and batch insertion are future work.

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

- Not yet implemented: edge removal and batch insertion.
