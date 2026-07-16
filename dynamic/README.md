# Feline-PK — Dynamic Reachability Index

Incremental (dynamic) extension of Feline, per Chapter 4 of the thesis. Maintains the
Feline index (two topological orderings / 2D coordinates) as the graph changes, instead
of rebuilding it on every update.

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

See `../docs/superpowers/specs/2026-07-15-feline-dynamic-index-design.md` for the design.

## Demo input format

One command per line:

    v <id>        insert a disconnected vertex
    e <u> <v>     insert edge (u, v)
    q <u> <v>     query reachability; prints "<u> <v> <0|1>"

## Known limitations / future work

- Cycle-closing insertions reposition the folded representative r' **locally** (thesis
  Alg. 10 lines 21–22) via bounded per-edge reorders over r''s incident E_DAG edges
  (I → r' → O), reusing the acyclic reorder machinery, instead of rebuilding the whole
  index. The residual O(size) cost is only the bulk member-removal compaction
  (`DynIndex::remove_many`, a single pass over the position arrays); a further gain
  would require gap-tolerant coordinates so that removal need not compact at all.
- Not yet implemented: edge removal (Alg. 11) and batch insertion (Alg. 12–14).
- Repeated insertion of many disconnected vertices before adding edges is O(n^2)
  overall because `append_isolated` reindexes the Y order on each call (fine for
  typical use; a future optimization can make it O(1) amortized).
